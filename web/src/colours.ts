import type { JSX } from "preact";
import type { ConsoleColours, PanelColour } from "./types";

export function renderColour(colour: PanelColour | undefined): JSX.CSSProperties {
  if (!colour) return {};
  const reverse = Boolean(colour.style & 2);
  return {
    color: reverse ? colour.background : colour.foreground,
    backgroundColor: reverse ? colour.foreground : colour.background,
    fontWeight: colour.style & 1 ? 700 : undefined,
    fontStyle: colour.style & 32 ? "italic" : undefined,
    textDecoration: [colour.style & 4 ? "underline" : "", colour.style & 512 ? "overline" : "", colour.style & 1024 ? "line-through" : ""].filter(Boolean).join(" ") || undefined,
    opacity: colour.style & 16 ? 0.65 : undefined,
  };
}

// CSS state selectors choose a control's available, selected or disabled role.
// Updating these values never replaces the console component or its drafts.
export function consoleColourVariables(colours: ConsoleColours | undefined): JSX.CSSProperties {
  const variables: JSX.CSSProperties = {};
  for (const [area, colour] of Object.entries(colours ?? {})) {
    const rendered = renderColour(colour);
    const style = {
      ...rendered,
      fontWeight: rendered.fontWeight ?? 400,
      fontStyle: rendered.fontStyle ?? "normal",
      textDecoration: rendered.textDecoration ?? "none",
      opacity: rendered.opacity ?? 1,
    };
    for (const [property, value] of Object.entries(style)) {
      if (value !== undefined)
        variables[`--console-${area}-${property.replace(/[A-Z]/g, (letter) => `-${letter.toLowerCase()}`)}`] = value;
    }
  }
  return variables;
}
