export type UploadState = "synced" | "processing" | "offline";

export type ImageItem = {
  id: string;
  uri: string;          // remote/local
  width: number;
  height: number;
  filename: string;
  uploadedState: UploadState;
  uploadProgress?: number; // 0-1 progress value
  processed?: boolean;
  favorite?: boolean;
  tags?: string[];
};
