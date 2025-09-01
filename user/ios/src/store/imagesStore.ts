import { create } from "zustand";
import AsyncStorage from "@react-native-async-storage/async-storage";
import { ImageItem } from "../types/images";

type ImagesState = {
  byProjectId: Record<string, ImageItem[]>;
  seed(projectId: string, items: ImageItem[]): void;
  toggleFavorite(projectId: string, id: string): void;
  getById(projectId: string, id: string): ImageItem | undefined;
  getByProject(projectId: string): ImageItem[];
  loadFromStorage(): Promise<void>;
  saveToStorage(): Promise<void>;
};

const STORAGE_KEY = "images_store";

export const useImagesStore = create<ImagesState>((set, get) => ({
  byProjectId: {},
  
  seed: (projectId, items) => {
    console.log('ImagesStore - Seeding project:', projectId, 'with', items.length, 'items');
    set((state) => ({
      byProjectId: {
        ...state.byProjectId,
        [projectId]: items
      }
    }));
    // Save to storage after seeding
    get().saveToStorage();
  },
  
  toggleFavorite: (projectId, id) => {
    console.log('ImagesStore - Toggling favorite for project:', projectId, 'image:', id);
    set((state) => {
      const projectImages = state.byProjectId[projectId] || [];
      const updatedImages = projectImages.map((it) => 
        it.id === id ? { ...it, favorite: !it.favorite } : it
      );
      
      return {
        byProjectId: {
          ...state.byProjectId,
          [projectId]: updatedImages
        }
      };
    });
    // Save to storage after toggling
    get().saveToStorage();
  },
  
  getById: (projectId, id) => {
    const projectImages = get().byProjectId[projectId] || [];
    return projectImages.find((i) => i.id === id);
  },
  
  getByProject: (projectId) => {
    return get().byProjectId[projectId] || [];
  },
  
  loadFromStorage: async () => {
    try {
      const stored = await AsyncStorage.getItem(STORAGE_KEY);
      if (stored) {
        const byProjectId = JSON.parse(stored);
        set({ byProjectId });
      }
    } catch (error) {
      console.warn("Failed to load images from storage:", error);
    }
  },
  
  saveToStorage: async () => {
    try {
      const { byProjectId } = get();
      await AsyncStorage.setItem(STORAGE_KEY, JSON.stringify(byProjectId));
    } catch (error) {
      console.warn("Failed to save images to storage:", error);
    }
  },
}));
