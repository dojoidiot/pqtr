import { create } from "zustand";

export type ExportFormat = "JPEG" | "TIFF" | "PNG";
export type Aspect = "1:1" | "4:5" | "3:2" | "16:9" | "9:16";
export type WatermarkPosition = "Top-Left"|"Top-Right"|"Bottom-Left"|"Bottom-Right"|"Center";

export type ProjectAutomation = {
  projectId: string;

  // Processing
  defaultPresetName?: string | null;
  autoApplyNew: boolean;
  retroApplyExisting: boolean;

  // Output
  exportFormat: ExportFormat;
  fileRule: string;              // e.g. "{project}_{date}_{counter}"
  counterStart: number;
  includeOriginalName: boolean;

  // Watermark
  enableWatermark: boolean;
  watermarkText: string;
  wmPosition: WatermarkPosition;
  wmOpacity: number;             // 0..100

  // Tagging & QC
  defaultTags: string;           // comma separated
  smartAutoTagging: boolean;
  minLongEdgePx: number;
  minExposureQuality: number;    // 0..100
  autoFlagLowQuality: boolean;

  // Aspect/Crops
  aspects: Aspect[];

  // Distribution
  storageChannels: string[];     // demo chips
  aiDistribution: boolean;
  aiNotes: string;

  // Scheduling (MVP demo switch)
  exportSchedulingOn: boolean;

  updatedAt: number;
};

type Store = {
  byId: Record<string, ProjectAutomation>;
  seed(projectId: string, partial?: Partial<ProjectAutomation>): void;
  get(projectId: string): ProjectAutomation | undefined;
  update(projectId: string, patch: Partial<ProjectAutomation>): void;
  replace(projectId: string, value: ProjectAutomation): void;
};

const defaults: Omit<ProjectAutomation,"projectId"|"updatedAt"> = {
  defaultPresetName: null,
  autoApplyNew: true,
  retroApplyExisting: false,
  exportFormat: "PNG",
  fileRule: "{project}_{date}_{counter}",
  counterStart: 1,
  includeOriginalName: true,
  enableWatermark: false,
  watermarkText: "© PQTR",
  wmPosition: "Bottom-Right",
  wmOpacity: 35,
  defaultTags: "",
  smartAutoTagging: false,
  minLongEdgePx: 2000,
  minExposureQuality: 0,
  autoFlagLowQuality: true,
  aspects: ["1:1","4:5","3:2","16:9"],
  storageChannels: [],
  aiDistribution: false,
  aiNotes: "",
  exportSchedulingOn: false,
};

export const useProjectAutomation = create<Store>((set, get) => ({
  byId: {},
  seed: (projectId, partial) =>
    set((s) => {
      if (s.byId[projectId]) return s;
      s.byId[projectId] = { projectId, ...defaults, ...partial, updatedAt: Date.now() };
      return { ...s };
    }),
  get: (projectId) => get().byId[projectId],
  update: (projectId, patch) =>
    set((s) => {
      const cur = s.byId[projectId] ?? { projectId, ...defaults, updatedAt: Date.now() };
      s.byId[projectId] = { ...cur, ...patch, updatedAt: Date.now() };
      return { ...s };
    }),
  replace: (projectId, value) =>
    set((s) => {
      s.byId[projectId] = { ...value, projectId, updatedAt: Date.now() };
      return { ...s };
    }),
}));
