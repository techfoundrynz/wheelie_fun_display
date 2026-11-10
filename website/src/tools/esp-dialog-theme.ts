// The pinned ESP Web Tools console hardcodes colors and exposes no theme parts.
// Add a small stylesheet to its open shadow root without changing vendor files.
const consoleStyles = `
  :host, input, .log {
    background-color: var(--bg);
    color: var(--text);
    font-family: ui-monospace, "Cascadia Code", Consolas, monospace;
  }
  :host {
    border: 1px solid var(--line);
    border-left: 3px solid var(--cyan);
    border-radius: 4px;
  }
  form { border-top: 1px solid var(--line); }
  input:focus-visible { outline: 2px solid var(--cyan); outline-offset: 2px; }
`;

export function observeEspConsoleTheme() {
  const dialogs = new Map<HTMLElement, MutationObserver>();
  const styled = new WeakSet<Element>();

  function themeConsoles(root: ShadowRoot) {
    for (const terminal of root.querySelectorAll("ewt-console")) {
      if (!terminal.shadowRoot || styled.has(terminal)) continue;
      const style =
        terminal.shadowRoot.querySelector<HTMLStyleElement>("style[data-wfd-console-theme]") ||
        document.createElement("style");
      style.dataset.wfdConsoleTheme = "";
      style.textContent = consoleStyles;
      if (!style.isConnected) terminal.shadowRoot.append(style);
      styled.add(terminal);
    }
  }

  function sync() {
    for (const [dialog, observer] of dialogs) {
      if (dialog.isConnected) continue;
      observer.disconnect();
      dialogs.delete(dialog);
    }
    for (const dialog of document.querySelectorAll<HTMLElement>("ewt-install-dialog")) {
      if (!dialog.shadowRoot || dialogs.has(dialog)) continue;
      const root = dialog.shadowRoot;
      themeConsoles(root);
      const observer = new MutationObserver(() => themeConsoles(root));
      observer.observe(root, { childList: true, subtree: true });
      dialogs.set(dialog, observer);
    }
  }

  sync();
  const observer = new MutationObserver(sync);
  observer.observe(document.body, { childList: true });
  return () => {
    observer.disconnect();
    for (const watcher of dialogs.values()) watcher.disconnect();
    dialogs.clear();
  };
}
