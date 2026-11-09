// Sirve los posters subidos desde el panel admin (guardados en Netlify Blobs).
import type { Config, Context } from "@netlify/functions";
import { getStore } from "@netlify/blobs";

export default async (_req: Request, context: Context) => {
  const nombre = context.params.archivo;
  const r = await getStore("posters").getWithMetadata(nombre, { type: "arrayBuffer" });
  if (!r) return new Response("No encontrado", { status: 404 });
  return new Response(r.data, {
    headers: {
      "Content-Type": String(r.metadata?.tipo ?? "image/jpeg"),
      "Cache-Control": "public, max-age=31536000, immutable",
    },
  });
};

export const config: Config = {
  path: "/img/peliculas/:archivo",
};
