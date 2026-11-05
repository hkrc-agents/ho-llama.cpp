export const run: (root: string, id: string) => Promise<string>;
export const cancel: () => boolean;
export const running: () => boolean;
