import React from 'react';
import { View, Text, TouchableOpacity, StyleSheet } from 'react-native';

const presets = ['None', 'Black & White', 'Grain + Flares', 'Cinematic'];

interface PresetSelectorProps {
  selected: string | null;
  onSelect: (preset: string) => void;
}

export default function PresetSelector({ selected, onSelect }: PresetSelectorProps) {
  return (
    <View style={styles.container}>
      <Text style={styles.label}>Select Preset</Text>
      <View style={styles.presetRow}>
        {presets.map((preset) => (
          <TouchableOpacity
            key={preset}
            style={[
              styles.preset,
              selected === preset && styles.activePreset,
            ]}
            onPress={() => onSelect(preset)}
          >
            <Text style={[
              styles.presetText,
              selected === preset && styles.activePresetText
            ]}>
              {preset}
            </Text>
          </TouchableOpacity>
        ))}
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { 
    marginBottom: 24 
  },
  label: { 
    marginBottom: 6, 
    fontSize: 14, 
    color: '#444' 
  },
  presetRow: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 10,
  },
  preset: {
    paddingVertical: 10,
    paddingHorizontal: 16,
    backgroundColor: '#EFEFEF',
    borderRadius: 12,
  },
  activePreset: {
    backgroundColor: '#175E4C',
  },
  presetText: {
    color: '#222',
    fontSize: 14,
    fontWeight: '500',
  },
  activePresetText: {
    color: '#fff',
  },
});
