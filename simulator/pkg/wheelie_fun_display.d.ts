/* tslint:disable */
/* eslint-disable */

export class WebSimulator {
    free(): void;
    [Symbol.dispose](): void;
    adjust_tuning(id: number, delta: number): void;
    edit_tuning(id: number): void;
    frame(milliseconds: number): void;
    invoke(callback: string): void;
    navigate(page: number): void;
    constructor(canvas_id: string);
    pointer(kind: string, x: number, y: number, milliseconds: number): void;
    scroll(x: number, y: number, delta: number): void;
    set_connected(connected: boolean): void;
    set_control(name: string, value: number): void;
    set_display_model(model: number): void;
    set_integration(id: number): void;
    set_rotation(degrees: number): void;
    set_tuning_section(section: number): void;
    show_pairing_demo(): void;
    state(name: string): any;
    /**
     * Deterministic illustrative data for generated docs pictures only.
     * Browser chrome continues to identify the preview as simulated hardware.
     */
    use_documentation_fixture(riding: boolean): void;
}

export type InitInput = RequestInfo | URL | Response | BufferSource | WebAssembly.Module;

export interface InitOutput {
    readonly memory: WebAssembly.Memory;
    readonly __wbg_websimulator_free: (a: number, b: number) => void;
    readonly websimulator_adjust_tuning: (a: number, b: number, c: number) => void;
    readonly websimulator_edit_tuning: (a: number, b: number) => void;
    readonly websimulator_frame: (a: number, b: number, c: number) => void;
    readonly websimulator_invoke: (a: number, b: number, c: number, d: number) => void;
    readonly websimulator_navigate: (a: number, b: number) => void;
    readonly websimulator_new: (a: number, b: number, c: number) => void;
    readonly websimulator_pointer: (a: number, b: number, c: number, d: number, e: number, f: number) => void;
    readonly websimulator_scroll: (a: number, b: number, c: number, d: number) => void;
    readonly websimulator_set_connected: (a: number, b: number) => void;
    readonly websimulator_set_control: (a: number, b: number, c: number, d: number) => void;
    readonly websimulator_set_display_model: (a: number, b: number) => void;
    readonly websimulator_set_integration: (a: number, b: number) => void;
    readonly websimulator_set_rotation: (a: number, b: number) => void;
    readonly websimulator_set_tuning_section: (a: number, b: number) => void;
    readonly websimulator_show_pairing_demo: (a: number) => void;
    readonly websimulator_state: (a: number, b: number, c: number) => number;
    readonly websimulator_use_documentation_fixture: (a: number, b: number) => void;
    readonly __wbindgen_export: (a: number, b: number, c: number) => void;
    readonly __wbindgen_export2: (a: number) => void;
    readonly __wbindgen_export3: (a: number, b: number) => number;
    readonly __wbindgen_export4: (a: number, b: number, c: number, d: number) => number;
    readonly __wbindgen_add_to_stack_pointer: (a: number) => number;
}

export type SyncInitInput = BufferSource | WebAssembly.Module;

/**
 * Instantiates the given `module`, which can either be bytes or
 * a precompiled `WebAssembly.Module`.
 *
 * @param {{ module: SyncInitInput }} module - Passing `SyncInitInput` directly is deprecated.
 *
 * @returns {InitOutput}
 */
export function initSync(module: { module: SyncInitInput } | SyncInitInput): InitOutput;

/**
 * If `module_or_path` is {RequestInfo} or {URL}, makes a request and
 * for everything else, calls `WebAssembly.instantiate` directly.
 *
 * @param {{ module_or_path: InitInput | Promise<InitInput> }} module_or_path - Passing `InitInput` directly is deprecated.
 *
 * @returns {Promise<InitOutput>}
 */
export default function __wbg_init (module_or_path?: { module_or_path: InitInput | Promise<InitInput> } | InitInput | Promise<InitInput>): Promise<InitOutput>;
