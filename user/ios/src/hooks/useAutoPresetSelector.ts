import { useCallback, useMemo } from 'react';
import { Preset, ImageMetadata } from '../types';

interface useAutoPresetSelectorProps {
  metadata: ImageMetadata;
  availablePresets: Preset[];
  onMatch: (presetId: string) => void;
}

interface PresetRule {
  id: string;
  name: string;
  conditions: (metadata: ImageMetadata) => boolean;
  priority: number;
  description: string;
}

export const useAutoPresetSelector = ({
  metadata,
  availablePresets,
  onMatch,
}: useAutoPresetSelectorProps) => {
  
  // Define intelligent preset selection rules
  const presetRules: PresetRule[] = useMemo(() => [
    {
      id: 'track-day',
      name: 'Track Day',
      priority: 10,
      description: 'High contrast, vibrant racing shots',
      conditions: (metadata: ImageMetadata) => {
        const location = metadata.location.toLowerCase();
        const iso = metadata.iso;
        const time = new Date(metadata.timestamp).getHours();
        
        return Boolean(
          (location.includes('pit') || location.includes('track') || location.includes('race')) &&
          (iso > 800 || time >= 6 && time <= 18)
        );
      },
    },
    {
      id: 'night-shooter',
      name: 'Night Shooter',
      priority: 9,
      description: 'Optimized for low-light night photography',
      conditions: (metadata: ImageMetadata) => {
        const time = new Date(metadata.timestamp).getHours();
        const shutterSpeed = metadata.shutterSpeed;
        const iso = metadata.iso;
        
        return Boolean(
          (time < 6 || time > 20) &&
          (shutterSpeed < 1/60 || iso > 1600)
        );
      },
    },
    {
      id: 'portrait-pro',
      name: 'Portrait Pro',
      priority: 8,
      description: 'Professional portrait enhancement',
      conditions: (metadata: ImageMetadata) => {
        const location = metadata.location.toLowerCase();
        const aperture = metadata.aperture;
        
        return Boolean(
          (location.includes('studio') || location.includes('portrait') || location.includes('headshot')) &&
          (aperture && aperture < 2.8)
        );
      },
    },
    {
      id: 'landscape-master',
      name: 'Landscape Master',
      priority: 7,
      description: 'Natural landscape photography',
      conditions: (metadata: ImageMetadata) => {
        const location = metadata.location.toLowerCase();
        const time = new Date(metadata.timestamp).getHours();
        
        return Boolean(
          (location.includes('mountain') || location.includes('forest') || location.includes('beach') || location.includes('park')) &&
          (time >= 6 && time <= 18)
        );
      },
    },
    {
      id: 'urban-edge',
      name: 'Urban Edge',
      priority: 6,
      description: 'Sharp, modern city photography',
      conditions: (metadata: ImageMetadata) => {
        const location = metadata.location.toLowerCase();
        const time = new Date(metadata.timestamp).getHours();
        
        return Boolean(
          (location.includes('city') || location.includes('urban') || location.includes('street') || location.includes('building')) &&
          (time >= 8 && time <= 20)
        );
      },
    },
    {
      id: 'vintage-film',
      name: 'Vintage Film',
      priority: 5,
      description: 'Classic film look with natural tones',
      conditions: (metadata: ImageMetadata) => {
        const iso = metadata.iso;
        const camera = metadata.camera?.toLowerCase() || '';
        
        return Boolean(
          (iso <= 400) ||
          (camera.includes('film') || camera.includes('analog'))
        );
      },
    },
    {
      id: 'sunset-glow',
      name: 'Sunset Glow',
      priority: 4,
      description: 'Warm, golden hour aesthetics',
      conditions: (metadata: ImageMetadata) => {
        const time = new Date(metadata.timestamp).getHours();
        const location = metadata.location.toLowerCase();
        
        return Boolean(
          ((time >= 16 && time <= 19) || (time >= 5 && time <= 8)) &&
          (location.includes('beach') || location.includes('mountain') || location.includes('park'))
        );
      },
    },
    {
      id: 'monochrome',
      name: 'Aston Mono',
      priority: 3,
      description: 'Monochrome with rich blacks',
      conditions: (metadata: ImageMetadata) => {
        const location = metadata.location.toLowerCase();
        const time = new Date(metadata.timestamp).getHours();
        
        return Boolean(
          (location.includes('street') || location.includes('architecture') || location.includes('portrait')) &&
          (time >= 10 && time <= 16)
        );
      },
    },
  ], []);

  // Find the best matching preset based on rules and available presets
  const findBestMatch = useCallback((): Preset | null => {
    const matchedRules = presetRules
      .filter(rule => rule.conditions(metadata))
      .sort((a, b) => b.priority - a.priority);

    if (matchedRules.length === 0) return null;

    // Find the highest priority rule that has a matching preset
    for (const rule of matchedRules) {
      const matchingPreset = availablePresets.find(preset => 
        preset.name.toLowerCase().includes(rule.name.toLowerCase()) ||
        preset.description.toLowerCase().includes(rule.description.toLowerCase())
      );
      
      if (matchingPreset) {
        return matchingPreset;
      }
    }

    return null;
  }, [metadata, availablePresets, presetRules]);

  // Auto-apply the best matching preset
  const autoApplyBestMatch = useCallback(() => {
    const bestMatch = findBestMatch();
    if (bestMatch) {
      onMatch(bestMatch.id);
      return bestMatch;
    }
    return null;
  }, [findBestMatch, onMatch]);

  // Get all matching rules for debugging/display
  const getMatchingRules = useCallback(() => {
    return presetRules
      .filter(rule => rule.conditions(metadata))
      .sort((a, b) => b.priority - a.priority);
  }, [metadata, presetRules]);

  // Get confidence score for a specific preset
  const getPresetConfidence = useCallback((preset: Preset): number => {
    const matchingRules = presetRules.filter(rule => 
      rule.conditions(metadata) &&
      (preset.name.toLowerCase().includes(rule.name.toLowerCase()) ||
       preset.description.toLowerCase().includes(rule.description.toLowerCase()))
    );
    
    if (matchingRules.length === 0) return 0;
    
    const maxPriority = Math.max(...matchingRules.map(r => r.priority));
    return (maxPriority / 10) * 100; // Convert to percentage
  }, [metadata, presetRules]);

  return {
    findBestMatch,
    autoApplyBestMatch,
    getMatchingRules,
    getPresetConfidence,
    hasMatches: presetRules.some(rule => rule.conditions(metadata)),
  };
};
