import React, { createContext, useContext, useState, useCallback, useEffect } from 'react';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { Preset, Image } from '../types';

interface PresetContextType {
  // State
  activePresetId: string | null;
  presets: Preset[];
  projectDefaultPresets: Record<string, string>; // projectId -> presetId
  
  // Actions
  setActivePresetId: (presetId: string | null) => void;
  addPreset: (preset: Preset) => void;
  updatePreset: (presetId: string, updates: Partial<Preset>) => void;
  deletePreset: (presetId: string) => void;
  duplicatePreset: (preset: Preset) => void;
  
  // Project defaults
  setProjectDefaultPreset: (projectId: string, presetId: string | null) => void;
  getProjectDefaultPreset: (projectId: string) => Preset | null;
  
  // Image operations
  applyPresetToImage: (imageId: string, presetId: string) => void;
  getPresetById: (presetId: string) => Preset | null;
  
  // Team sharing
  togglePresetSharing: (presetId: string, shared: boolean) => void;
  
  // Versioning
  createPresetVersion: (presetId: string, settings: any, changes: string[]) => void;
  rollbackToVersion: (presetId: string, versionId: string) => void;
}

const PresetContext = createContext<PresetContextType | undefined>(undefined);

interface PresetProviderProps {
  children: React.ReactNode;
}

const STORAGE_KEYS = {
  ACTIVE_PRESET: 'pqtr_active_preset',
  PRESETS: 'pqtr_presets',
  PROJECT_DEFAULTS: 'pqtr_project_defaults',
};

export const PresetProvider: React.FC<PresetProviderProps> = ({ children }) => {
  const [activePresetId, setActivePresetIdState] = useState<string | null>(null);
  const [presets, setPresetsState] = useState<Preset[]>([]);
  const [projectDefaultPresets, setProjectDefaultPresetsState] = useState<Record<string, string>>({});

  // Load data from storage on mount
  useEffect(() => {
    loadStoredData();
  }, []);

  const loadStoredData = async () => {
    try {
      const [storedActivePreset, storedPresets, storedProjectDefaults] = await Promise.all([
        AsyncStorage.getItem(STORAGE_KEYS.ACTIVE_PRESET),
        AsyncStorage.getItem(STORAGE_KEYS.PRESETS),
        AsyncStorage.getItem(STORAGE_KEYS.PROJECT_DEFAULTS),
      ]);

      if (storedActivePreset) {
        setActivePresetIdState(storedActivePreset);
      }

      if (storedPresets) {
        setPresetsState(JSON.parse(storedPresets));
      }

      if (storedProjectDefaults) {
        setProjectDefaultPresetsState(JSON.parse(storedProjectDefaults));
      }
    } catch (error) {
      console.error('Error loading preset data:', error);
    }
  };

  const setActivePresetId = useCallback(async (presetId: string | null) => {
    setActivePresetIdState(presetId);
    try {
      if (presetId) {
        await AsyncStorage.setItem(STORAGE_KEYS.ACTIVE_PRESET, presetId);
      } else {
        await AsyncStorage.removeItem(STORAGE_KEYS.ACTIVE_PRESET);
      }
    } catch (error) {
      console.error('Error saving active preset:', error);
    }
  }, []);

  const addPreset = useCallback(async (preset: Preset) => {
    setPresetsState(prev => [preset, ...prev]);
    try {
      const updatedPresets = [preset, ...presets];
      await AsyncStorage.setItem(STORAGE_KEYS.PRESETS, JSON.stringify(updatedPresets));
    } catch (error) {
      console.error('Error saving preset:', error);
    }
  }, [presets]);

  const updatePreset = useCallback(async (presetId: string, updates: Partial<Preset>) => {
    setPresetsState(prev => 
      prev.map(p => p.id === presetId ? { ...p, ...updates } : p)
    );
    
    try {
      const updatedPresets = presets.map(p => p.id === presetId ? { ...p, ...updates } : p);
      await AsyncStorage.setItem(STORAGE_KEYS.PRESETS, JSON.stringify(updatedPresets));
    } catch (error) {
      console.error('Error updating preset:', error);
    }
  }, [presets]);

  const deletePreset = useCallback(async (presetId: string) => {
    setPresetsState(prev => prev.filter(p => p.id !== presetId));
    
    // Remove from project defaults if it was set
    setProjectDefaultPresetsState(prev => {
      const updated = { ...prev };
      Object.keys(updated).forEach(projectId => {
        if (updated[projectId] === presetId) {
          delete updated[projectId];
        }
      });
      return updated;
    });
    
    try {
      const updatedPresets = presets.filter(p => p.id !== presetId);
      await AsyncStorage.setItem(STORAGE_KEYS.PRESETS, JSON.stringify(updatedPresets));
      
      // Update project defaults storage
      const updatedDefaults = { ...projectDefaultPresets };
      Object.keys(updatedDefaults).forEach(projectId => {
        if (updatedDefaults[projectId] === presetId) {
          delete updatedDefaults[projectId];
        }
      });
      await AsyncStorage.setItem(STORAGE_KEYS.PROJECT_DEFAULTS, JSON.stringify(updatedDefaults));
    } catch (error) {
      console.error('Error deleting preset:', error);
    }
  }, [presets, projectDefaultPresets]);

  const duplicatePreset = useCallback(async (preset: Preset) => {
    const duplicatedPreset: Preset = {
      ...preset,
      id: Date.now().toString(),
      name: `${preset.name} (Copy)`,
      createdAt: new Date().toISOString(),
      lastEdited: new Date().toISOString(),
      version: 1,
      appliedCount: 0,
      isActive: false,
      sharedWithTeam: false,
    };
    
    addPreset(duplicatedPreset);
  }, [addPreset]);

  const setProjectDefaultPreset = useCallback(async (projectId: string, presetId: string | null) => {
    setProjectDefaultPresetsState(prev => {
      const updated = { ...prev };
      if (presetId) {
        updated[projectId] = presetId;
      } else {
        delete updated[projectId];
      }
      return updated;
    });
    
    try {
      const updatedDefaults = { ...projectDefaultPresets };
      if (presetId) {
        updatedDefaults[projectId] = presetId;
      } else {
        delete updatedDefaults[projectId];
      }
      await AsyncStorage.setItem(STORAGE_KEYS.PROJECT_DEFAULTS, JSON.stringify(updatedDefaults));
    } catch (error) {
      console.error('Error saving project default preset:', error);
    }
  }, [projectDefaultPresets]);

  const getProjectDefaultPreset = useCallback((projectId: string): Preset | null => {
    const presetId = projectDefaultPresets[projectId];
    return presetId ? presets.find(p => p.id === presetId) || null : null;
  }, [projectDefaultPresets, presets]);

  const applyPresetToImage = useCallback((imageId: string, presetId: string) => {
    // This would typically update the image in your data store
    // For now, we'll just track that the preset was applied
    console.log(`Applied preset ${presetId} to image ${imageId}`);
  }, []);

  const getPresetById = useCallback((presetId: string): Preset | null => {
    return presets.find(p => p.id === presetId) || null;
  }, [presets]);

  const togglePresetSharing = useCallback(async (presetId: string, shared: boolean) => {
    updatePreset(presetId, { sharedWithTeam: shared });
  }, [updatePreset]);

  const createPresetVersion = useCallback(async (presetId: string, settings: any, changes: string[]) => {
    const preset = presets.find(p => p.id === presetId);
    if (!preset) return;

    const newVersion: Preset = {
      ...preset,
      version: preset.version + 1,
      lastEdited: new Date().toISOString(),
      settings,
      previousVersions: [
        ...(preset.previousVersions || []),
        {
          id: `${presetId}_v${preset.version}`,
          version: preset.version,
          createdAt: preset.lastEdited,
          settings: preset.settings,
          changes: [],
        }
      ],
    };

    updatePreset(presetId, newVersion);
  }, [presets, updatePreset]);

  const rollbackToVersion = useCallback(async (presetId: string, versionId: string) => {
    const preset = presets.find(p => p.id === presetId);
    if (!preset || !preset.previousVersions) return;

    const targetVersion = preset.previousVersions.find(v => v.id === versionId);
    if (!targetVersion) return;

    const rolledBackPreset: Preset = {
      ...preset,
      version: preset.version + 1,
      lastEdited: new Date().toISOString(),
      settings: targetVersion.settings,
      previousVersions: [
        ...preset.previousVersions,
        {
          id: `${presetId}_v${preset.version}`,
          version: preset.version,
          createdAt: preset.lastEdited,
          settings: preset.settings,
          changes: [`Rolled back to version ${targetVersion.version}`],
        }
      ],
    };

    updatePreset(presetId, rolledBackPreset);
  }, [presets, updatePreset]);

  const value: PresetContextType = {
    activePresetId,
    presets,
    projectDefaultPresets,
    setActivePresetId,
    addPreset,
    updatePreset,
    deletePreset,
    duplicatePreset,
    setProjectDefaultPreset,
    getProjectDefaultPreset,
    applyPresetToImage,
    getPresetById,
    togglePresetSharing,
    createPresetVersion,
    rollbackToVersion,
  };

  return (
    <PresetContext.Provider value={value}>
      {children}
    </PresetContext.Provider>
  );
};

export const usePresetContext = (): PresetContextType => {
  const context = useContext(PresetContext);
  if (context === undefined) {
    throw new Error('usePresetContext must be used within a PresetProvider');
  }
  return context;
};
