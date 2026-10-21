import {
  Env,
  bearer,
  safeEqual,
  verifyStripeSignature,
  toDevicePayment,
} from "./stripe";

export { Hub } from "./hub";

// Every device talks to the same singleton Hub instance.
const hub = (env: Env) => env.HUB.get(env.HUB.idFromName("main"));

function deviceAuthed(req: Request, env: Env): boolean {
  const t = bearer(req) ?? new URL(req.url).searchParams.get("token");
  return !!t && safeEqual(t, env.DEVICE_TOKEN);
}

export default {
  async fetch(req: Request, env: Env, ctx: ExecutionContext): Promise<Response> {
    const url = new URL(req.url);

    // ---- Stripe webhook ---------------------------------------------------
    if (req.method === "POST" && url.pathname === "/webhook/stripe") {
      const raw = await req.text();
      const ok = await verifyStripeSignature(
        raw,
        req.headers.get("stripe-signature"),
        env.STRIPE_WEBHOOK_SECRET,
      );
      if (!ok) return new Response("bad signature", { status: 400 });

      let event: any;
      try { event = JSON.parse(raw); } catch { return new Response("bad json", { status: 400 }); }

      const payment = toDevicePayment(event);
      if (payment && payment.cents > 0) {
        // Ack Stripe immediately; fan out to the device in the background so a
        // slow DO can never cause a webhook retry.
        ctx.waitUntil(
          hub(env).fetch("https://hub/push", {
            method: "POST",
            body: JSON.stringify(payment),
          }),
        );
      }
      return new Response("ok");
    }

    // ---- device stream ----------------------------------------------------
    if (url.pathname === "/v1/stream") {
      if (!deviceAuthed(req, env)) return new Response("unauthorized", { status: 401 });
      return hub(env).fetch("https://hub/stream");
    }

    // ---- one-shot stats (handy for curl / a second display) ---------------
    if (url.pathname === "/v1/snapshot") {
      if (!deviceAuthed(req, env)) return new Response("unauthorized", { status: 401 });
      const force = url.searchParams.get("force") === "1" ? "?force=1" : "";
      return hub(env).fetch(`https://hub/snapshot${force}`);
    }

    // ---- fire a fake sale so you can watch the animation ------------------
    if (req.method === "POST" && url.pathname === "/v1/test") {
      if (!deviceAuthed(req, env)) return new Response("unauthorized", { status: 401 });
      const cents = Number(url.searchParams.get("cents") ?? 4200);
      await hub(env).fetch("https://hub/push", {
        method: "POST",
        body: JSON.stringify({
          id: `test-${Date.now()}`,
          cents,
          who: url.searchParams.get("who") ?? "test@example.com",
          what: url.searchParams.get("what") ?? "VISA 4242",
          refund: url.searchParams.get("refund") === "1",
        }),
      });
      return new Response("sent");
    }

    if (url.pathname === "/") return new Response("stripe-dot-relay up");
    return new Response("not found", { status: 404 });
  },
} satisfies ExportedHandler<Env>;
