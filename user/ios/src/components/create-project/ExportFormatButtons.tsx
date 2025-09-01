import React from 'react';
import { View, Text, TouchableOpacity, StyleSheet } from 'react-native';
import { useSharedValue, useAnimatedStyle, withSpring } from 'react-native-reanimated';
import Animated from 'react-native-reanimated';

interface ExportFormat {
  id: string;
  name: string;
  ratio: string;
}

interface ExportFormatButtonsProps {
  selectedFormats: string[];
  onSelectFormat: (formatId: string) => void;
}

const exportFormats: ExportFormat[] = [
  {
    id: '9x16',
    name: 'Portrait',
    ratio: '9:16'
  },
  {
    id: '3x4',
    name: 'Portrait',
    ratio: '3:4'
  },
  {
    id: '1x1',
    name: 'Square',
    ratio: '1:1'
  },
  {
    id: '4x3',
    name: 'Landscape',
    ratio: '4:3'
  },
  {
    id: '16x9',
    name: 'Landscape',
    ratio: '16:9'
  }
];

const FormatButton = ({ format, isSelected, onSelectFormat }: { 
  format: ExportFormat; 
  isSelected: boolean; 
  onSelectFormat: (formatId: string) => void; 
}) => {
  const scale = useSharedValue(isSelected ? 1.02 : 1);
  
  React.useEffect(() => {
    scale.value = withSpring(isSelected ? 1.02 : 1, {
      damping: 15,
      stiffness: 300,
    });
  }, [isSelected]);

  const animatedStyle = useAnimatedStyle(() => ({
    transform: [{ scale: scale.value }],
  }));

  return (
    <Animated.View style={[styles.formatContainer, animatedStyle]}>
      <TouchableOpacity
        style={[
          styles.formatButton,
          isSelected && styles.formatButtonSelected
        ]}
        onPress={() => onSelectFormat(format.id)}
        activeOpacity={0.8}
      >
        <Text style={[
          styles.formatName,
          isSelected && styles.formatNameSelected
        ]}>
          {format.name}
        </Text>
        <Text style={[
          styles.formatRatio,
          isSelected && styles.formatRatioSelected
        ]}>
          {format.ratio}
        </Text>
      </TouchableOpacity>
    </Animated.View>
  );
};

export default function ExportFormatButtons({ selectedFormats = [], onSelectFormat }: ExportFormatButtonsProps) {
  return (
    <View style={styles.container}>
      <Text style={styles.label}>Export Format</Text>
      <View style={styles.formatsGrid}>
        {exportFormats.map((format) => (
          <FormatButton
            key={format.id}
            format={format}
            isSelected={selectedFormats.includes(format.id)}
            onSelectFormat={onSelectFormat}
          />
        ))}
      </View>
      {selectedFormats.length > 0 && (
        <Text style={styles.selectionInfo}>
          Selected: {selectedFormats.length} format{selectedFormats.length > 1 ? 's' : ''}
        </Text>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    marginBottom: 32,
    paddingHorizontal: 24,
  },
  label: {
    fontSize: 16,
    fontWeight: '500',
    color: '#444',
    marginBottom: 16,
  },
  formatsGrid: {
    flexDirection: 'row',
    gap: 12,
    justifyContent: 'space-between',
  },
  formatContainer: {
    flex: 1,
  },
  formatButton: {
    backgroundColor: '#F8F8F8',
    borderRadius: 12,
    paddingVertical: 16,
    paddingHorizontal: 12,
    alignItems: 'center',
    borderWidth: 1,
    borderColor: '#F0F0F0',
    minHeight: 70,
  },
  formatButtonSelected: {
    backgroundColor: '#175E4C',
    borderColor: '#175E4C',
    shadowColor: '#175E4C',
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.2,
    shadowRadius: 8,
    elevation: 6,
  },
  formatName: {
    fontSize: 14,
    fontWeight: '600',
    color: '#222',
    marginBottom: 4,
  },
  formatNameSelected: {
    color: '#FFF',
  },
  formatRatio: {
    fontSize: 12,
    fontWeight: '500',
    color: '#666',
  },
  formatRatioSelected: {
    color: '#E0E0E0',
  },
  selectionInfo: {
    fontSize: 14,
    color: '#666',
    marginTop: 12,
    textAlign: 'center',
  },
});
