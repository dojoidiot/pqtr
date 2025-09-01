import React from 'react';
import { View, Text, TouchableOpacity, StyleSheet } from 'react-native';
import { FlatList } from 'react-native';

interface PresetCarouselProps {
  presets: string[];
  selectedPreset: string;
  onPresetSelect: (preset: string) => void;
}

export default function PresetCarousel({ presets, selectedPreset, onPresetSelect }: PresetCarouselProps) {
  const renderPreset = ({ item, index }: { item: string; index: number }) => {
    const isSelected = selectedPreset === item;

    return (
      <View style={styles.presetContainer}>
        <TouchableOpacity
          style={[
            styles.presetButton,
            isSelected && styles.presetButtonSelected
          ]}
          onPress={() => onPresetSelect(item)}
          activeOpacity={0.8}
        >
          <Text style={[
            styles.presetName,
            isSelected && styles.presetNameSelected
          ]}>
            {item}
          </Text>
        </TouchableOpacity>
      </View>
    );
  };

  return (
    <View style={styles.container}>
      <FlatList
        data={presets}
        renderItem={renderPreset}
        keyExtractor={(item) => item}
        horizontal
        showsHorizontalScrollIndicator={false}
        contentContainerStyle={styles.carouselContainer}
        snapToInterval={140}
        decelerationRate="fast"
        snapToAlignment="center"
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    marginBottom: 0,
  },
  presetContainer: {
    marginRight: 12,
  },
  presetButton: {
    paddingHorizontal: 16,
    paddingVertical: 12,
    backgroundColor: 'rgba(255, 255, 255, 0.9)',
    borderRadius: 20,
    borderWidth: 1,
    borderColor: 'rgba(0, 0, 0, 0.1)',
    minWidth: 120,
    alignItems: 'center',
  },
  presetButtonSelected: {
    backgroundColor: '#175E4C',
    borderColor: '#175E4C',
  },
  presetName: {
    fontSize: 14,
    fontWeight: '500',
    color: '#333',
    textAlign: 'center',
  },
  presetNameSelected: {
    color: '#FFF',
    fontWeight: '600',
  },
  carouselContainer: {
    paddingHorizontal: 20,
  },
});
