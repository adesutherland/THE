import { useEffect, useRef, useState } from "preact/hooks";
import type { Snapshot } from "./types";
import type { EditorSettings } from "./tool-session";

export function SettingsDialog({ snapshot, settings, online, onClose, onCommand, onEditProfile, onTools }: {
  snapshot: Snapshot | null; settings?: EditorSettings; online: boolean;
  onClose: () => void; onCommand: (command: string) => void; onEditProfile: () => void; onTools: () => void;
}) {
  const [section, setSection] = useState<"appearance" | "editor">("appearance");
  const [tabWidth, setTabWidth] = useState(String(settings?.tabIncrement || 8));
  const root = useRef<HTMLDivElement>(null);
  useEffect(() => { root.current?.querySelector<HTMLButtonElement>("button")?.focus(); }, []);
  useEffect(() => { if (settings?.tabIncrement) setTabWidth(String(settings.tabIncrement)); }, [settings?.tabIncrement]);
  const disabled = !online || !settings || !!snapshot?.panel;
  const toggle = (name: string, enabled: boolean) => onCommand(`SET ${name} ${enabled ? "ON" : "OFF"}`);
  return <div class="dialog-backdrop" role="presentation" onMouseDown={(event) => { if (event.target === event.currentTarget) onClose(); }}>
    <div class="settings-dialog" ref={root} role="dialog" aria-modal="true" aria-labelledby="settings-title"
      onKeyDown={(event) => {
        if (event.key === "Escape") { event.preventDefault(); event.stopPropagation(); onClose(); }
        if (event.key === "Tab") {
          const controls = [...event.currentTarget.querySelectorAll<HTMLElement>("button:not(:disabled), input:not(:disabled), select:not(:disabled), a[href]")];
          const first = controls[0], last = controls[controls.length - 1];
          if (event.shiftKey && document.activeElement === first) { event.preventDefault(); last?.focus(); }
          else if (!event.shiftKey && document.activeElement === last) { event.preventDefault(); first?.focus(); }
        }
      }}>
      <div class="dialog-heading"><h2 id="settings-title">Settings</h2><button type="button" onClick={onClose}>Close</button></div>
      <div class="settings-body">
        <nav class="settings-nav" aria-label="Settings sections">
          <button type="button" aria-pressed={section === "appearance"} onClick={() => setSection("appearance")}>Appearance</button>
          <button type="button" aria-pressed={section === "editor"} onClick={() => setSection("editor")}>Editor</button>
          <button type="button" onClick={onTools}>Tools & Profiles ↗</button>
        </nav>
        <section class="settings-content" aria-label={section === "appearance" ? "Appearance settings" : "Editor settings"}>
          {section === "appearance" ? <>
            <h3>Text size</h3><p>Use browser zoom to change text size. Your browser owns this preference.</p>
            <h3>Editor colours and key guide</h3><p>Defined by the active profile.</p>
            <button type="button" disabled={!online || !!snapshot?.panel} onClick={onEditProfile}>Edit profile</button>
          </> : <>
            <h3>Current file</h3><p class="settings-scope">{snapshot?.buffer?.path || "No file selected"}</p>
            <p>These commands apply to the current file. Edit profile defaults to set startup behaviour.</p>
            {!settings ? <p role="status">Current editor settings are unavailable from this host.</p> : <>
              <label><input type="checkbox" checked={settings.lineNumbers} disabled={disabled} onChange={(event) => toggle("NUMBER", event.currentTarget.checked)} /> Show line numbers</label>
              <label><input type="checkbox" checked={settings.prefix} disabled={disabled} onChange={(event) => toggle("PREFIX", event.currentTarget.checked)} /> Show prefix area</label>
              <label><input type="checkbox" checked={settings.wordwrap} disabled={disabled} onChange={(event) => toggle("WORDWRAP", event.currentTarget.checked)} /> Word wrap</label>
              <p>Typing mode: {settings.insertMode ? "Insert" : "Overwrite"}. Use your profile’s Insert key binding to change it.</p>
              <label class="settings-tab-width">Tab increment<input aria-label="Tab increment" type="number" min="1" max="32" value={tabWidth} disabled={disabled}
                onInput={(event) => setTabWidth(event.currentTarget.value)} /></label>
              {settings.tabIncrement === 0 ? <p>Custom tab stops are active. Applying an increment replaces those stops.</p> : null}
              <button type="button" disabled={disabled || !/^\d+$/.test(tabWidth) || Number(tabWidth) < 1 || Number(tabWidth) > 32}
                onClick={() => onCommand(`SET TABS INCR ${Number(tabWidth)}`)}>Apply tab increment</button>
              <p class="tools-hint">Setting provenance: profile-defined or unknown.</p>
            </>}
            <button type="button" disabled={!online || !!snapshot?.panel} onClick={onEditProfile}>Edit profile defaults</button>
          </>}
        </section>
      </div>
      <div class="dialog-actions"><button type="button" onClick={onClose}>Done</button></div>
    </div>
  </div>;
}
