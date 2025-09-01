import { create } from "zustand";

type SelState = {
  selecting: boolean;
  selectedIds: string[];
  enter(): void;              // turn on select mode
  exit(): void;               // turn off select mode + clear
  toggle(id: string): void;   // toggle selection
  select(id: string): void;   // add single id
  clear(): void;              // clear selected, keep mode state
  set(ids: string[]): void;
};

export const useSelection = create<SelState>((set, get) => ({
  selecting: false,
  selectedIds: [],
  enter: () => set({ selecting: true }),
  exit: () => set({ selecting: false, selectedIds: [] }),
  toggle: (id) => {
    const cur = get().selectedIds;
    const next = cur.includes(id) ? cur.filter(x => x !== id) : [...cur, id];
    set({ selectedIds: next });
  },
  select: (id) => {
    const cur = get().selectedIds;
    if (!cur.includes(id)) set({ selectedIds: [...cur, id] });
  },
  clear: () => set({ selectedIds: [] }),
  set: (ids) => set({ selecting: ids.length > 0, selectedIds: ids }),
}));
