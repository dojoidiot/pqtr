// store/projectAutomation.ts
// Minimal local store for automation settings (swap with your real store later).

import { useEffect, useState } from "react";

export type ExportFormat = "JPEG" | "TIFF" | "PNG";
export type RatioKey = "1:1" | "4:5" | "3:2" | "16:9" | "9:16";
export type DistChannel = "PQTR_Archive" | "PQTR_ClientDelivery" | "PQTR_Newsroom" | "PQTR_SocialMedia" | "PQTR_Print";

export interface ProjectAutomationSettings {
  projectId: string;

  // Processing
  defaultPresetId?: string | null;
  presetChain: string[];             // chain of preset IDs in order
  autoApplyToNew: boolean;
  retroApplyExisting: boolean;

  // File Output
  exportFormat: ExportFormat;
  fileNamingRule: string;            // supports tokens like {project} {date} {counter} {tag}
  counterStart: number;
  includeOriginalName: boolean;

  // Aspect Ratios
  aspectRatios: RatioKey[];          // selected output variants (thumbnails show)

  // Distribution
  distributionChannels: DistChannel[];    // PQTR internal storage rules
  aiDistribution: boolean;                // future: AI routing
  aiNotes?: string;

  // Watermark
  watermarkEnabled: boolean;
  watermarkText?: string;
  watermarkOpacity: number;          // 0..1
  watermarkPosition: "TL"|"TR"|"BL"|"BR"|"C";

  // Auto Tagging
  defaultTags: string[];
  smartTagging: boolean;

  // Quality Control
  minResolution: number;             // px long edge min
  minExposureScore?: number;         // 0..100 (optional QC metric)
  autoFlagLowQuality: boolean;

  // Versioning
  keepOriginal: boolean;             // non-destructive
  writeEditsToNewCopy: boolean;

  // Scheduling
  exportSchedule: "off" | "hourly" | "daily_18" | "end_of_day";
}

const DEFAULTS: Omit<ProjectAutomationSettings, "projectId"> = {
  defaultPresetId: null,
  presetChain: [],
  autoApplyToNew: true,
  retroApplyExisting: false,

  exportFormat: "JPEG",
  fileNamingRule: "{project}_{date}_{counter}",
  counterStart: 1,
  includeOriginalName: false,

  aspectRatios: ["3:2", "4:5"],

  distributionChannels: [],
  aiDistribution: false,
  aiNotes: "",

  watermarkEnabled: false,
  watermarkText: "",
  watermarkOpacity: 0.35,
  watermarkPosition: "BR",

  defaultTags: [],
  smartTagging: false,

  minResolution: 2000,
  minExposureScore: undefined,
  autoFlagLowQuality: true,

  keepOriginal: true,
  writeEditsToNewCopy: true,

  exportSchedule: "off",
};

export function useProjectAutomation(projectId: string) {
  const [settings, setSettings] = useState<ProjectAutomationSettings>({
    projectId,
    ...DEFAULTS,
  });

  // TODO: hydrate from persistent store / backend
  useEffect(() => {
    // fetch(projectId) -> setSettings(...)
  }, [projectId]);

  const update = <K extends keyof ProjectAutomationSettings>(key: K, value: ProjectAutomationSettings[K]) => {
    setSettings(prev => ({ ...prev, [key]: value }));
    // TODO: persist (debounced)
  };

  const batch = (patch: Partial<ProjectAutomationSettings>) => {
    setSettings(prev => ({ ...prev, ...patch }));
    // TODO: persist (debounced)
  };

  return { settings, update, batch };
}
