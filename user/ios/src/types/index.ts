export interface Project {
  id: string;
  title: string;
  defaultPresetId?: string;
  images: Image[];
  createdAt: Date;
  lastSynced: Date;
}

export interface Image {
  id: string;
  url: string | any; // Can be string URL or local asset
  thumbnail: string | any; // Can be string URL or local asset
  title?: string;
  tags: string[];
  isPublished: boolean;
  presetId?: string;
  metadata?: ImageMetadata;
}

export interface ImageMetadata {
  iso: number;
  shutterSpeed: number;
  location: string;
  timestamp: string;
  camera?: string;
  lens?: string;
  aperture?: number;
}

export interface StorageInfo {
  used: number;
  total: number;
  percentage: number;
}

export interface PresetSettings {
  brightness: number;
  contrast: number;
  saturation: number;
  temperature: number;
  tint: number;
  highlights: number;
  shadows: number;
  sharpness: number;
  vignette: number;
}

export interface PresetVersion {
  id: string;
  version: number;
  createdAt: string;
  settings: PresetSettings;
  changes: string[];
}

export interface Preset {
  id: string;
  name: string;
  description: string;
  thumbnailUri: string;
  createdAt: string;
  lastEdited: string;
  version: number;
  sharedWithTeam: boolean;
  createdBy: string;
  appliedCount: number;
  isActive: boolean;
  previousVersions?: PresetVersion[];
  settings: PresetSettings;
} 