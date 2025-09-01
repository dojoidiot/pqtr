import React from 'react';
import {
  View,
  Text,
  TouchableOpacity,
  Image,
  StyleSheet,
  Dimensions,
  ScrollView,
  Modal,
  Animated,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { theme } from '../constants/theme';

interface Preset {
  id: string;
  name: string;
  thumbnail: string;
  description: string;
}

interface PresetSelectorModalProps {
  isVisible: boolean;
  onClose: () => void;
  onSelectPreset: (preset: Preset) => void;
  activePreset?: Preset;
}

const samplePresets: Preset[] = [
  {
    id: '1',
    name: 'Track Day',
    thumbnail: 'https://images.unsplash.com/photo-1541899481282-d53bffe3c35d?w=150&h=150&fit=crop',
    description: 'High contrast, vibrant racing shots',
  },
  {
    id: '2',
    name: 'Neutral Film',
    thumbnail: 'https://images.unsplash.com/photo-1551698618-1dfe5d97d256?w=150&h=150&fit=crop',
    description: 'Classic film look with natural tones',
  },
  {
    id: '3',
    name: 'Aston Mono',
    thumbnail: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=150&h=150&fit=crop',
    description: 'Monochrome with rich blacks',
  },
  {
    id: '4',
    name: 'Sunset Glow',
    thumbnail: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=150&h=150&fit=crop',
    description: 'Warm, golden hour aesthetics',
  },
  {
    id: '5',
    name: 'Urban Edge',
    thumbnail: 'https://images.unsplash.com/photo-1541899481282-d53bffe3c35d?w=150&h=150&fit=crop',
    description: 'Sharp, modern city photography',
  },
  {
    id: '6',
    name: 'Portrait Pro',
    thumbnail: 'https://images.unsplash.com/photo-1551698618-1dfe5d97d256?w=150&h=150&fit=crop',
    description: 'Professional portrait enhancement',
  },
];

const { width: screenWidth } = Dimensions.get('window');

const PresetSelectorModal: React.FC<PresetSelectorModalProps> = ({
  isVisible,
  onClose,
  onSelectPreset,
  activePreset,
}) => {
  const slideAnim = React.useRef(new Animated.Value(300)).current;

  React.useEffect(() => {
    if (isVisible) {
      Animated.spring(slideAnim, {
        toValue: 0,
        useNativeDriver: true,
        tension: 100,
        friction: 8,
      }).start();
    } else {
      slideAnim.setValue(300);
    }
  }, [isVisible]);

  const handlePresetSelect = (preset: Preset) => {
    onSelectPreset(preset);
    onClose();
  };

  const handleBackdropPress = () => {
    Animated.timing(slideAnim, {
      toValue: 300,
      duration: 200,
      useNativeDriver: true,
    }).start(() => onClose());
  };

  return (
    <Modal
      visible={isVisible}
      transparent={true}
      animationType="none"
      onRequestClose={onClose}
    >
      <TouchableOpacity
        style={styles.backdrop}
        activeOpacity={1}
        onPress={handleBackdropPress}
      >
        <Animated.View
          style={[
            styles.container,
            {
              transform: [{ translateY: slideAnim }],
            },
          ]}
        >
          {/* Handle Bar */}
          <View style={styles.handleBar}>
            <View style={styles.handle} />
          </View>

          {/* Header */}
          <View style={styles.header}>
            <Text style={styles.title}>Select Preset</Text>
            <TouchableOpacity onPress={onClose} style={styles.closeButton}>
              <Ionicons name="close" size={24} color={theme.colors.text.secondary} />
            </TouchableOpacity>
          </View>

          {/* Presets Grid */}
          <ScrollView style={styles.presetsContainer} showsVerticalScrollIndicator={false}>
            <View style={styles.presetsGrid}>
              {samplePresets.map((preset) => (
                <TouchableOpacity
                  key={preset.id}
                  style={[
                    styles.presetItem,
                    activePreset?.id === preset.id && styles.presetItemActive,
                  ]}
                  onPress={() => handlePresetSelect(preset)}
                  accessibilityLabel={`Select ${preset.name} preset`}
                  accessibilityRole="button"
                >
                  <Image source={{ uri: preset.thumbnail }} style={styles.presetThumbnail} />
                  <View style={styles.presetInfo}>
                    <Text style={styles.presetName}>{preset.name}</Text>
                    <Text style={styles.presetDescription} numberOfLines={2}>
                      {preset.description}
                    </Text>
                  </View>
                  {activePreset?.id === preset.id && (
                    <View style={styles.checkmarkContainer}>
                      <Ionicons name="checkmark-circle" size={24} color={theme.colors.status.success} />
                    </View>
                  )}
                </TouchableOpacity>
              ))}
            </View>
          </ScrollView>
        </Animated.View>
      </TouchableOpacity>
    </Modal>
  );
};

const styles = StyleSheet.create({
  backdrop: {
    flex: 1,
    backgroundColor: 'rgba(0, 0, 0, 0.5)',
    justifyContent: 'flex-end',
  },
  container: {
    backgroundColor: theme.colors.surface,
    borderTopLeftRadius: theme.borderRadius.xl,
    borderTopRightRadius: theme.borderRadius.xl,
    paddingTop: theme.spacing.m,
    maxHeight: '80%',
  },
  handleBar: {
    alignItems: 'center',
    marginBottom: theme.spacing.m,
  },
  handle: {
    width: 40,
    height: 4,
    backgroundColor: theme.colors.border,
    borderRadius: 2,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.l,
    marginBottom: theme.spacing.l,
  },
  title: {
    fontSize: theme.fontSizes.title,
    fontWeight: '700',
    color: theme.colors.text.primary,
  },
  closeButton: {
    padding: theme.spacing.s,
    borderRadius: theme.borderRadius.s,
    backgroundColor: theme.colors.lightGray,
  },
  presetsContainer: {
    paddingHorizontal: theme.spacing.l,
  },
  presetsGrid: {
    gap: theme.spacing.m,
    paddingBottom: theme.spacing.xl,
  },
  presetItem: {
    flexDirection: 'row',
    alignItems: 'center',
    padding: theme.spacing.m,
    backgroundColor: theme.colors.background,
    borderRadius: theme.borderRadius.l,
    borderWidth: 1,
    borderColor: theme.colors.border,
  },
  presetItemActive: {
    borderColor: theme.colors.status.success,
    backgroundColor: 'rgba(52, 199, 89, 0.05)',
  },
  presetThumbnail: {
    width: 60,
    height: 60,
    borderRadius: theme.borderRadius.m,
    marginRight: theme.spacing.m,
  },
  presetInfo: {
    flex: 1,
  },
  presetName: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '600',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.xs,
  },
  presetDescription: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.secondary,
    lineHeight: 18,
  },
  checkmarkContainer: {
    marginLeft: theme.spacing.s,
  },
});

export default PresetSelectorModal;
