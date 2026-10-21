export interface Env {
  HUB: DurableObjectNamespace;
  STRIPE_SECRET_KEY: string;
  STRIPE_WEBHOOK_SECRET: string;
  DEVICE_TOKEN: string;
  TZ_OFFSET_SECONDS: string;
  ENABLE_MRR: string;
}

const API = "https://api.stripe.com";

// --- auth helpers -----------------------------------------------------------

/** Length-independent constant-time-ish compare. */
export function safeEqual(a: string, b: string): boolean {
  const ab = new TextEncoder().encode(a);
  const bb = new TextEncoder().encode(b);
  let diff = ab.length ^ bb.length;
  const n = Math.max(ab.length, bb.length);
  for (let i = 0; i < n; i++) diff |= (ab[i] ?? 0) ^ (bb[i] ?? 0);
  return diff === 0;
}

export function bearer(req: Request): string | null {
  const h = req.headers.get("authorization") ?? "";
  return h.startsWith("Bearer ") ? h.slice(7) : null;
}

/**
 * Verifies Stripe's `Stripe-Signature` header against the raw request body.
 * Mirrors the scheme Stripe's own libraries implement: HMAC-SHA256 over
 * `${timestamp}.${payload}`, hex-compared against any of the v1 signatures,
 * with a replay window.
 */
export async function verifyStripeSignature(
  payload: string,
  header: string | null,
  secret: string,
  toleranceSeconds = 300,
): Promise<boolean> {
  if (!header) return false;

  let ts = "";
  const sigs: string[] = [];
  for (const part of header.split(",")) {
    const [k, v] = part.split("=", 2);
    if (k?.trim() === "t") ts = v ?? "";
    else if (k?.trim() === "v1" && v) sigs.push(v);
  }
  if (!ts || sigs.length === 0) return false;

  const age = Math.abs(Math.floor(Date.now() / 1000) - Number(ts));
  if (!Number.isFinite(age) || age > toleranceSeconds) return false;

  const key = await crypto.subtle.importKey(
    "raw",
    new TextEncoder().encode(secret),
    { name: "HMAC", hash: "SHA-256" },
    false,
    ["sign"],
  );
  const mac = await crypto.subtle.sign(
    "HMAC",
    key,
    new TextEncoder().encode(`${ts}.${payload}`),
  );
  const expected = [...new Uint8Array(mac)]
    .map((b) => b.toString(16).padStart(2, "0"))
    .join("");

  return sigs.some((s) => safeEqual(s, expected));
}

// --- Stripe REST ------------------------------------------------------------

async function api(env: Env, path: string): Promise<any> {
  const r = await fetch(`${API}${path}`, {
    headers: {
      Authorization: `Bearer ${env.STRIPE_SECRET_KEY}`,
      "Stripe-Version": "2024-06-20",
    },
  });
  if (!r.ok) throw new Error(`stripe ${path} -> ${r.status} ${await r.text()}`);
  return r.json();
}

const MON = ["JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"];

/** Local-midnight epoch for the day containing `epoch`, in the configured TZ. */
function dayStart(epoch: number, tzOffset: number): number {
  const local = epoch + tzOffset;
  return local - (local % 86400) - tzOffset;
}

export interface Snapshot {
  today: { cents: number; count: number };
  available: number;
  pending: number;
  payout: { cents: number; when: string };
  days: number[];
  mrr: number;
  subs: number;
  epoch: number;
}

export async function buildSnapshot(env: Env): Promise<Snapshot> {
  const tz = Number(env.TZ_OFFSET_SECONDS) || 0;
  const now = Math.floor(Date.now() / 1000);
  const today0 = dayStart(now, tz);
  const from = today0 - 13 * 86400; // 14 buckets, oldest first

  const days = new Array(14).fill(0);
  let todayCents = 0;
  let todayCount = 0;

  // Charges since the start of the window. Capped at 5 pages so a busy account
  // can't stall the Worker; the chart degrades gracefully if you exceed it.
  let starting = "";
  for (let page = 0; page < 5; page++) {
    const q = `/v1/charges?limit=100&created[gte]=${from}${starting}`;
    const res = await api(env, q);
    for (const ch of res.data as any[]) {
      if (ch.status !== "succeeded" || !ch.paid) continue;
      const net = (ch.amount ?? 0) - (ch.amount_refunded ?? 0);
      if (net <= 0) continue;
      const idx = Math.floor((dayStart(ch.created, tz) - from) / 86400);
      if (idx >= 0 && idx < 14) days[idx] += net;
      if (ch.created >= today0) {
        todayCents += net;
        todayCount += 1;
      }
    }
    if (!res.has_more || res.data.length === 0) break;
    starting = `&starting_after=${res.data[res.data.length - 1].id}`;
  }

  const balance = await api(env, "/v1/balance");
  const sum = (arr: any[]) =>
    (arr ?? []).reduce((a: number, b: any) => a + (b.amount ?? 0), 0);

  let payoutCents = 0;
  let payoutWhen = "";
  try {
    const po = await api(env, "/v1/payouts?limit=3");
    const next = (po.data as any[]).find(
      (p) => p.status === "pending" || p.status === "in_transit",
    );
    if (next) {
      payoutCents = next.amount ?? 0;
      const d = new Date((next.arrival_date ?? 0) * 1000);
      payoutWhen = `${MON[d.getUTCMonth()]} ${d.getUTCDate()}`;
    }
  } catch { /* payouts can be unavailable on some accounts */ }

  let mrr = 0;
  let subs = 0;
  if (env.ENABLE_MRR === "1") {
    try {
      let after = "";
      for (let page = 0; page < 5; page++) {
        const res = await api(env, `/v1/subscriptions?status=active&limit=100${after}`);
        for (const s of res.data as any[]) {
          subs += 1;
          for (const it of s.items?.data ?? []) {
            const p = it.price;
            if (!p?.unit_amount) continue;
            const qty = it.quantity ?? 1;
            const iv = p.recurring?.interval;
            const ic = p.recurring?.interval_count ?? 1;
            const per =
              iv === "year" ? 12 * ic : iv === "week" ? ic / 4.345 :
              iv === "day" ? ic / 30.44 : ic;
            mrr += Math.round((p.unit_amount * qty) / per);
          }
        }
        if (!res.has_more || res.data.length === 0) break;
        after = `&starting_after=${res.data[res.data.length - 1].id}`;
      }
    } catch { /* not every account has Billing */ }
  }

  return {
    today: { cents: todayCents, count: todayCount },
    available: sum(balance.available),
    pending: sum(balance.pending),
    payout: { cents: payoutCents, when: payoutWhen },
    days,
    mrr,
    subs,
    epoch: now,
  };
}

// --- webhook -> device payload ---------------------------------------------

export interface DevicePayment {
  id: string;
  cents: number;
  who: string;
  what: string;
  refund: boolean;
}

/** Flattens the Stripe events we care about into the device's tiny shape. */
export function toDevicePayment(event: any): DevicePayment | null {
  const o = event?.data?.object;
  if (!o) return null;

  const card = (pm: any) =>
    pm?.card ? `${(pm.card.brand ?? "CARD").toUpperCase()} ${pm.card.last4 ?? ""}`.trim() : "";

  switch (event.type) {
    case "charge.succeeded":
      return {
        id: o.id,
        cents: o.amount ?? 0,
        who: o.billing_details?.email || o.billing_details?.name || "",
        what: o.description || card(o.payment_method_details) || "",
        refund: false,
      };

    case "charge.refunded":
      return {
        id: `${o.id}:refund:${o.amount_refunded}`,
        cents: o.amount_refunded ?? 0,
        who: o.billing_details?.email || o.billing_details?.name || "",
        what: o.description || "",
        refund: true,
      };

    case "checkout.session.completed":
      if (o.payment_status !== "paid") return null;
      return {
        id: o.payment_intent || o.id,
        cents: o.amount_total ?? 0,
        who: o.customer_details?.email || o.customer_details?.name || "",
        what: "CHECKOUT",
        refund: false,
      };

    case "payment_intent.succeeded":
      // Fallback for flows that don't emit a charge we can see. The Hub
      // de-dupes against the charge id so this never double-fires.
      return {
        id: o.latest_charge || o.id,
        cents: o.amount_received ?? o.amount ?? 0,
        who: o.receipt_email || "",
        what: o.description || "",
        refund: false,
      };
  }
  return null;
}
