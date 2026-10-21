import { Env, buildSnapshot, Snapshot, DevicePayment } from "./stripe";

const HEARTBEAT_MS = 20_000;
const STATS_MAX_AGE_MS = 60_000;
const DEDUPE_TTL_MS = 10 * 60_000;

/**
 * One Durable Object holds every open device stream plus the cached snapshot.
 * Webhooks land on the Worker, get verified, and are pushed in here; the
 * device sees them within a few hundred milliseconds of Stripe firing.
 */
export class Hub implements DurableObject {
  private clients = new Set<WritableStreamDefaultWriter<Uint8Array>>();
  private snapshot: Snapshot | null = null;
  private snapshotAt = 0;
  private seen = new Map<string, number>();
  private enc = new TextEncoder();

  constructor(private ctx: DurableObjectState, private env: Env) {}

  async fetch(req: Request): Promise<Response> {
    const url = new URL(req.url);

    if (url.pathname === "/stream") return this.openStream();

    if (url.pathname === "/push") {
      const p = (await req.json()) as DevicePayment;
      await this.onPayment(p);
      return new Response("ok");
    }

    if (url.pathname === "/snapshot") {
      const s = await this.getSnapshot(url.searchParams.get("force") === "1");
      return Response.json(s);
    }

    return new Response("not found", { status: 404 });
  }

  // --- stream ---------------------------------------------------------------

  private async openStream(): Promise<Response> {
    const { readable, writable } = new TransformStream<Uint8Array, Uint8Array>();
    const w = writable.getWriter();
    this.clients.add(w);

    // Greet with a comment so the device's idle timer starts clean, then the
    // current stats so the first card can render without a round trip.
    await this.write(w, `: connected\n\n`);
    this.getSnapshot(false)
      .then((s) => this.write(w, this.sse("stats", s)))
      .catch(() => {});

    await this.ctx.storage.setAlarm(Date.now() + HEARTBEAT_MS);

    return new Response(readable, {
      headers: {
        "content-type": "text/event-stream; charset=utf-8",
        "cache-control": "no-cache, no-transform",
        connection: "keep-alive",
        "x-accel-buffering": "no",
      },
    });
  }

  private sse(event: string, data: unknown): string {
    return `event: ${event}\ndata: ${JSON.stringify(data)}\n\n`;
  }

  private async write(w: WritableStreamDefaultWriter<Uint8Array>, s: string) {
    try {
      await w.write(this.enc.encode(s));
    } catch {
      this.clients.delete(w);
      try { await w.close(); } catch {}
    }
  }

  private async broadcast(s: string) {
    await Promise.all([...this.clients].map((w) => this.write(w, s)));
  }

  // --- events ---------------------------------------------------------------

  private async onPayment(p: DevicePayment) {
    const now = Date.now();
    for (const [k, t] of this.seen) if (now - t > DEDUPE_TTL_MS) this.seen.delete(k);
    if (this.seen.has(p.id)) return;
    this.seen.set(p.id, now);

    await this.broadcast(
      this.sse("payment", {
        cents: p.cents,
        who: p.who,
        what: p.what,
        refund: p.refund,
      }),
    );

    // Roll the cached stats forward immediately so the device's "today" total
    // is already correct behind the alert, then reconcile with Stripe shortly
    // after (fees, holds and refunds can all move the real numbers).
    if (this.snapshot) {
      const d = p.refund ? -p.cents : p.cents;
      this.snapshot.today.cents += d;
      if (!p.refund) this.snapshot.today.count += 1;
      const last = this.snapshot.days.length - 1;
      if (last >= 0) this.snapshot.days[last] = Math.max(0, this.snapshot.days[last] + d);
      await this.broadcast(this.sse("stats", this.snapshot));
    }
    this.snapshotAt = 0; // force a real refresh on the next alarm
  }

  private async getSnapshot(force: boolean): Promise<Snapshot> {
    const fresh = !force && this.snapshot && Date.now() - this.snapshotAt < STATS_MAX_AGE_MS;
    if (fresh) return this.snapshot!;
    try {
      this.snapshot = await buildSnapshot(this.env);
      this.snapshotAt = Date.now();
    } catch (e) {
      if (!this.snapshot) throw e;   // nothing cached to fall back on
    }
    return this.snapshot!;
  }

  // --- heartbeat ------------------------------------------------------------

  async alarm() {
    if (this.clients.size === 0) return;      // nobody listening, let the DO idle

    await this.broadcast(`: ping\n\n`);

    if (Date.now() - this.snapshotAt >= STATS_MAX_AGE_MS) {
      try {
        const s = await this.getSnapshot(true);
        await this.broadcast(this.sse("stats", s));
      } catch { /* keep the stream alive even if Stripe hiccups */ }
    }

    if (this.clients.size > 0)
      await this.ctx.storage.setAlarm(Date.now() + HEARTBEAT_MS);
  }
}
