import React, { useState } from 'react';
import {
  View,
  Text,
  TouchableOpacity,
  StyleSheet,
  ScrollView,
  Alert,
  Switch,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

interface ExportModalProps {
  isVisible: boolean;
  onClose: () => void;
  selectedImages: string[];
  onExport: (exportConfig: ExportConfig) => void;
}

interface ExportConfig {
  format: 'JPEG' | 'PNG' | 'TIFF';
  resolution: 'Original' | '4K' | '2K' | '1080p';
  destination: 'FTP' | 'Drive' | 'Email' | 'Social';
  quality: number;
  includeMetadata: boolean;
  watermark: boolean;
}

const ExportModal: React.FC<ExportModalProps> = ({
  isVisible,
  onClose,
  selectedImages,
  onExport,
}) => {
  const [exportConfig, setExportConfig] = useState<ExportConfig>({
    format: 'JPEG',
    resolution: 'Original',
    destination: 'Drive',
    quality: 90,
    includeMetadata: true,
    watermark: false,
  });

  const [selectedPreset, setSelectedPreset] = useState<string | null>(null);

  const exportPresets = [
    { id: 'web', name: 'Web Ready', format: 'JPEG', resolution: '1080p', quality: 80 },
    { id: 'print', name: 'Print Quality', format: 'TIFF', resolution: 'Original', quality: 100 },
    { id: 'social', name: 'Social Media', format: 'JPEG', resolution: '2K', quality: 85 },
    { id: 'archive', name: 'Archive', format: 'PNG', resolution: 'Original', quality: 100 },
  ];

  const handleExport = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    
    if (selectedImages.length === 0) {
      Alert.alert('No Images Selected', 'Please select at least one image to export.');
      return;
    }

    onExport(exportConfig);
    onClose();
  };

  const updateConfig = (key: keyof ExportConfig, value: any) => {
    setExportConfig(prev => ({ ...prev, [key]: value }));
  };

  const applyPreset = (preset: any) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setExportConfig(prev => ({
      ...prev,
      format: preset.format,
      resolution: preset.resolution,
      quality: preset.quality,
    }));
    setSelectedPreset(preset.id);
  };

  if (!isVisible) return null;

  return (
    <View style={styles.overlay}>
      <View style={styles.modal}>
        {/* Header */}
        <View style={styles.header}>
          <Text style={styles.title}>Export Images</Text>
          <TouchableOpacity onPress={onClose} style={styles.closeButton}>
            <Ionicons name="close" size={24} color={theme.colors.text.secondary} />
          </TouchableOpacity>
        </View>

        <ScrollView style={styles.content} showsVerticalScrollIndicator={false}>
          {/* Export Presets */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Export Presets</Text>
            <View style={styles.presetGrid}>
              {exportPresets.map((preset) => (
                <TouchableOpacity
                  key={preset.id}
                  style={[
                    styles.presetCard,
                    selectedPreset === preset.id && styles.presetCardSelected
                  ]}
                  onPress={() => applyPreset(preset)}
                >
                  <Text style={styles.presetName}>{preset.name}</Text>
                  <Text style={styles.presetDetails}>
                    {preset.format} • {preset.resolution} • {preset.quality}%
                  </Text>
                </TouchableOpacity>
              ))}
            </View>
          </View>

          {/* Format Selection */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Format</Text>
            <View style={styles.optionGrid}>
              {(['JPEG', 'PNG', 'TIFF'] as const).map((format) => (
                <TouchableOpacity
                  key={format}
                  style={[
                    styles.optionButton,
                    exportConfig.format === format && styles.optionButtonSelected
                  ]}
                  onPress={() => updateConfig('format', format)}
                >
                  <Text style={[
                    styles.optionText,
                    exportConfig.format === format && styles.optionTextSelected
                  ]}>
                    {format}
                  </Text>
                </TouchableOpacity>
              ))}
            </View>
          </View>

          {/* Resolution Selection */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Resolution</Text>
            <View style={styles.optionGrid}>
              {(['Original', '4K', '2K', '1080p'] as const).map((resolution) => (
                <TouchableOpacity
                  key={resolution}
                  style={[
                    styles.optionButton,
                    exportConfig.resolution === resolution && styles.optionButtonSelected
                  ]}
                  onPress={() => updateConfig('resolution', resolution)}
                >
                  <Text style={[
                    styles.optionText,
                    exportConfig.resolution === resolution && styles.optionTextSelected
                  ]}>
                    {resolution}
                  </Text>
                </TouchableOpacity>
              ))}
            </View>
          </View>

          {/* Quality Slider */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Quality: {exportConfig.quality}%</Text>
            <View style={styles.qualitySlider}>
              <TouchableOpacity
                style={styles.qualityButton}
                onPress={() => updateConfig('quality', Math.max(50, exportConfig.quality - 10))}
              >
                <Ionicons name="remove" size={20} color={theme.colors.text.secondary} />
              </TouchableOpacity>
              <View style={styles.qualityBar}>
                <View style={[styles.qualityFill, { width: `${exportConfig.quality}%` }]} />
              </View>
              <TouchableOpacity
                style={styles.qualityButton}
                onPress={() => updateConfig('quality', Math.min(100, exportConfig.quality + 10))}
              >
                <Ionicons name="add" size={20} color={theme.colors.text.secondary} />
              </TouchableOpacity>
            </View>
          </View>

          {/* Destination Selection */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Export Destination</Text>
            <View style={styles.destinationGrid}>
              {(['FTP', 'Drive', 'Email', 'Social'] as const).map((destination) => (
                <TouchableOpacity
                  key={destination}
                  style={[
                    styles.destinationCard,
                    exportConfig.destination === destination && styles.destinationCardSelected
                  ]}
                  onPress={() => updateConfig('destination', destination)}
                >
                  <Ionicons 
                    name={getDestinationIcon(destination)} 
                    size={24} 
                    color={exportConfig.destination === destination ? theme.colors.surface : theme.colors.text.secondary} 
                  />
                  <Text style={[
                    styles.destinationText,
                    exportConfig.destination === destination && styles.destinationTextSelected
                  ]}>
                    {destination}
                  </Text>
                </TouchableOpacity>
              ))}
            </View>
          </View>

          {/* Options */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Options</Text>
            <View style={styles.optionRow}>
              <Text style={styles.optionLabel}>Include Metadata</Text>
              <Switch
                value={exportConfig.includeMetadata}
                onValueChange={(value) => updateConfig('includeMetadata', value)}
                trackColor={{ false: theme.colors.lightGray, true: theme.colors.green }}
                thumbColor={theme.colors.surface}
              />
            </View>
            <View style={styles.optionRow}>
              <Text style={styles.optionLabel}>Add Watermark</Text>
              <Switch
                value={exportConfig.watermark}
                onValueChange={(value) => updateConfig('watermark', value)}
                trackColor={{ false: theme.colors.lightGray, true: theme.colors.green }}
                thumbColor={theme.colors.surface}
              />
            </View>
          </View>

          {/* Summary */}
          <View style={styles.summary}>
            <Text style={styles.summaryTitle}>Export Summary</Text>
            <Text style={styles.summaryText}>
              {selectedImages.length} image{selectedImages.length !== 1 ? 's' : ''} • {exportConfig.format} • {exportConfig.resolution} • {exportConfig.quality}%
            </Text>
            <Text style={styles.summaryDestination}>
              To: {exportConfig.destination}
            </Text>
          </View>
        </ScrollView>

        {/* Action Buttons */}
        <View style={styles.actions}>
          <TouchableOpacity style={styles.cancelButton} onPress={onClose}>
            <Text style={styles.cancelButtonText}>Cancel</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.exportButton} onPress={handleExport}>
            <Ionicons name="download-outline" size={20} color={theme.colors.surface} />
            <Text style={styles.exportButtonText}>
              Export ({selectedImages.length})
            </Text>
          </TouchableOpacity>
        </View>
      </View>
    </View>
  );
};

const getDestinationIcon = (destination: string): keyof typeof Ionicons.glyphMap => {
  switch (destination) {
    case 'FTP': return 'server-outline';
    case 'Drive': return 'cloud-upload-outline';
    case 'Email': return 'mail-outline';
    case 'Social': return 'share-social-outline';
    default: return 'cloud-upload-outline';
  }
};

const styles = StyleSheet.create({
  overlay: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: 'rgba(0, 0, 0, 0.5)',
    justifyContent: 'flex-end',
    zIndex: 1000,
  },
  modal: {
    backgroundColor: theme.colors.surface,
    borderTopLeftRadius: theme.borderRadius.xl,
    borderTopRightRadius: theme.borderRadius.xl,
    maxHeight: '90%',
    ...theme.shadows.large,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: theme.spacing.l,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  title: {
    fontSize: theme.fontSizes.title,
    fontWeight: '700',
    color: theme.colors.text.primary,
  },
  closeButton: {
    padding: theme.spacing.s,
  },
  content: {
    padding: theme.spacing.l,
  },
  section: {
    marginBottom: theme.spacing.xl,
  },
  sectionTitle: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '600',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.m,
  },
  presetGrid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: theme.spacing.s,
  },
  presetCard: {
    flex: 1,
    minWidth: '45%',
    padding: theme.spacing.m,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    borderWidth: 2,
    borderColor: 'transparent',
  },
  presetCardSelected: {
    borderColor: theme.colors.green,
    backgroundColor: theme.colors.green + '10',
  },
  presetName: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.xs,
  },
  presetDetails: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
  },
  optionGrid: {
    flexDirection: 'row',
    gap: theme.spacing.s,
  },
  optionButton: {
    flex: 1,
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.m,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    alignItems: 'center',
    borderWidth: 2,
    borderColor: 'transparent',
  },
  optionButtonSelected: {
    backgroundColor: theme.colors.green,
    borderColor: theme.colors.green,
  },
  optionText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  optionTextSelected: {
    color: theme.colors.surface,
  },
  qualitySlider: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.m,
  },
  qualityButton: {
    padding: theme.spacing.s,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.s,
  },
  qualityBar: {
    flex: 1,
    height: 8,
    backgroundColor: theme.colors.lightGray,
    borderRadius: 4,
    overflow: 'hidden',
  },
  qualityFill: {
    height: '100%',
    backgroundColor: theme.colors.green,
    borderRadius: 4,
  },
  destinationGrid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: theme.spacing.s,
  },
  destinationCard: {
    flex: 1,
    minWidth: '45%',
    padding: theme.spacing.m,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    alignItems: 'center',
    gap: theme.spacing.s,
    borderWidth: 2,
    borderColor: 'transparent',
  },
  destinationCardSelected: {
    backgroundColor: theme.colors.green,
    borderColor: theme.colors.green,
  },
  destinationText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  destinationTextSelected: {
    color: theme.colors.surface,
  },
  optionRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: theme.spacing.s,
  },
  optionLabel: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    fontWeight: '500',
  },
  summary: {
    backgroundColor: theme.colors.lightGray,
    padding: theme.spacing.m,
    borderRadius: theme.borderRadius.m,
    marginBottom: theme.spacing.l,
  },
  summaryTitle: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '600',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.s,
  },
  summaryText: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.secondary,
    marginBottom: theme.spacing.xs,
  },
  summaryDestination: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    fontWeight: '500',
  },
  actions: {
    flexDirection: 'row',
    padding: theme.spacing.l,
    gap: theme.spacing.m,
    borderTopWidth: 1,
    borderTopColor: theme.colors.border,
  },
  cancelButton: {
    flex: 1,
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.l,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    alignItems: 'center',
  },
  cancelButtonText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  exportButton: {
    flex: 2,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.l,
    backgroundColor: theme.colors.green,
    borderRadius: theme.borderRadius.m,
    gap: theme.spacing.s,
  },
  exportButtonText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.surface,
  },
});

export default ExportModal;
