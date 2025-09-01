import React, { useState, useEffect } from 'react';
import {
  View,
  Text,
  Modal,
  TouchableOpacity,
  StyleSheet,
  Dimensions,
  ScrollView,
  Image,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { useSharedValue, useAnimatedStyle, withTiming } from 'react-native-reanimated';

const { width: screenWidth, height: screenHeight } = Dimensions.get('window');

// Mock preset data based on your LRPRESETS folder
const PRESETS = [
  'Ammyslife',
  'Black and White 1',
  'Black and White 2',
  'Grain Boost',
  'Moody Night',
  'Cinematic',
  'Vintage',
  'Clean',
  'Warm',
  'Cool',
  'High Contrast',
  'Soft',
  'Dramatic',
  'Natural',
  'Artistic',
];

interface PresetLivePreviewModalProps {
  visible: boolean;
  onClose: () => void;
  selectedPreset: string;
  onPresetSelect: (preset: string) => void;
}

export default function PresetLivePreviewModal({
  visible,
  onClose,
  selectedPreset,
  onPresetSelect,
}: PresetLivePreviewModalProps) {
  const [imageUri, setImageUri] = useState<string>('');
  const [presetStrength, setPresetStrength] = useState(100);

  const modalOpacity = useSharedValue(0);
  const modalScale = useSharedValue(0.9);

  useEffect(() => {
    if (visible) {
      modalOpacity.value = withTiming(1, { duration: 300 });
      modalScale.value = withTiming(1, { duration: 300 });
    } else {
      modalOpacity.value = withTiming(0, { duration: 200 });
      modalScale.value = withTiming(0.9, { duration: 200 });
    }
  }, [visible]);

  useEffect(() => {
    // Using one of the user's actual JPEG images from assets
    setImageUri('https://images.unsplash.com/photo-1503023345310-bd7c1de61c7d?w=1200');
  }, []);

  const handlePresetSelect = (preset: string) => {
    onPresetSelect(preset);
  };

  const animatedModalStyle = useAnimatedStyle(() => ({
    opacity: modalOpacity.value,
    transform: [{ scale: modalScale.value }],
  }));

  return (
    <Modal
      visible={visible}
      transparent
      animationType="none"
      onRequestClose={onClose}
    >
      <View style={styles.overlay}>
        <View style={[styles.modal, animatedModalStyle]}>
          {/* Header */}
          <View style={styles.header}>
            <Text style={styles.title}>Live Preset Preview</Text>
            <TouchableOpacity onPress={onClose} style={styles.closeButton}>
              <Ionicons name="close" size={24} color="#666" />
            </TouchableOpacity>
          </View>

          {/* Live Preview Surface */}
          <View style={styles.previewContainer}>
            {imageUri ? (
              <View style={styles.imageContainer}>
                <Image 
                  source={{ uri: imageUri }} 
                  style={styles.previewImage}
                  resizeMode="cover"
                />
                <View style={styles.presetOverlay}>
                  <Text style={styles.presetName}>{selectedPreset}</Text>
                  <Text style={styles.presetStrength}>Strength: {presetStrength}%</Text>
                </View>
              </View>
            ) : (
              <View style={styles.placeholderContainer}>
                <Ionicons name="image-outline" size={64} color="#CCC" />
                <Text style={styles.placeholderText}>Loading preview...</Text>
              </View>
            )}
          </View>

          {/* Preset Selection Bar */}
          <View style={styles.presetBar}>
            <ScrollView 
              horizontal 
              showsHorizontalScrollIndicator={false}
              contentContainerStyle={styles.presetScrollContent}
            >
              {PRESETS.map((preset) => (
                <TouchableOpacity
                  key={preset}
                  style={[
                    styles.presetItem,
                    selectedPreset === preset && styles.presetItemSelected
                  ]}
                  onPress={() => handlePresetSelect(preset)}
                >
                  <Text style={[
                    styles.presetItemText,
                    selectedPreset === preset && styles.presetItemTextSelected
                  ]}>
                    {preset}
                  </Text>
                </TouchableOpacity>
              ))}
            </ScrollView>
          </View>

          {/* Info Section */}
          <View style={styles.infoSection}>
            <Text style={styles.infoTitle}>Preset Effects Applied:</Text>
            <Text style={styles.infoText}>
              • {selectedPreset} preset at {presetStrength}% strength
            </Text>
            <Text style={styles.infoNote}>
              Note: This is a preview. Actual preset effects will be applied during export.
            </Text>
          </View>
        </View>
      </View>
    </Modal>
  );
}

const styles = StyleSheet.create({
  overlay: {
    flex: 1,
    backgroundColor: 'rgba(0, 0, 0, 0.5)',
    justifyContent: 'center',
    alignItems: 'center',
  },
  modal: {
    width: screenWidth * 0.9,
    maxHeight: screenHeight * 0.8,
    backgroundColor: '#FFF',
    borderRadius: 20,
    overflow: 'hidden',
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    padding: 20,
    borderBottomWidth: 1,
    borderBottomColor: '#F0F0F0',
  },
  title: {
    fontSize: 18,
    fontWeight: '600',
    color: '#222',
  },
  closeButton: {
    padding: 4,
  },
  previewContainer: {
    height: 300,
    backgroundColor: '#F8F8F8',
  },
  imageContainer: {
    flex: 1,
    position: 'relative',
  },
  previewImage: {
    width: '100%',
    height: '100%',
  },
  presetOverlay: {
    position: 'absolute',
    bottom: 20,
    left: 20,
    backgroundColor: 'rgba(0, 0, 0, 0.7)',
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 12,
  },
  presetName: {
    color: '#FFF',
    fontSize: 16,
    fontWeight: '600',
  },
  presetStrength: {
    color: '#CCC',
    fontSize: 12,
    marginTop: 2,
  },
  placeholderContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  placeholderText: {
    marginTop: 16,
    fontSize: 16,
    color: '#999',
  },
  presetBar: {
    paddingVertical: 16,
    borderBottomWidth: 1,
    borderBottomColor: '#F0F0F0',
  },
  presetScrollContent: {
    paddingHorizontal: 20,
  },
  presetItem: {
    paddingHorizontal: 16,
    paddingVertical: 8,
    backgroundColor: '#F5F5F5',
    borderRadius: 16,
    marginRight: 12,
    borderWidth: 1,
    borderColor: '#E0E0E0',
  },
  presetItemSelected: {
    backgroundColor: '#175E4C',
    borderColor: '#175E4C',
  },
  presetItemText: {
    fontSize: 14,
    color: '#666',
    fontWeight: '500',
  },
  presetItemTextSelected: {
    color: '#FFF',
    fontWeight: '600',
  },
  infoSection: {
    padding: 20,
  },
  infoTitle: {
    fontSize: 16,
    fontWeight: '600',
    color: '#222',
    marginBottom: 12,
  },
  infoText: {
    fontSize: 14,
    color: '#666',
    marginBottom: 6,
  },
  infoNote: {
    fontSize: 12,
    color: '#999',
    fontStyle: 'italic',
    marginTop: 12,
    paddingTop: 12,
    borderTopWidth: 1,
    borderTopColor: '#F0F0F0',
  },
});
