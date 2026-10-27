import { defineConfig } from "vite";
import preact from "@preact/preset-vite";

export default defineConfig({
  plugins: [preact()],
  define: { "import.meta.env.VITE_THE_BACKEND": JSON.stringify(process.env.THE_WEB_BACKEND ?? "native") },
  build: {
    target: "es2022",
    sourcemap: true,
    assetsInlineLimit: 4096,
  },
});
