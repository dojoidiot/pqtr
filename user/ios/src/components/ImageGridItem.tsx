import React, { useRef } from 'react';
import { TouchableOpacity, Image, Text, View, Animated, StyleSheet, Dimensions } from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

interface ImageGridItemProps {
  imageUrl: string;
  tagCount: number;
  isPublished: boolean;
  onPress: () => void;
  isSelected?: boolean;
  presetName?: string;
  isSynced?: boolean;
  isEdited?: boolean;
  filename?: string;
}

const { width: screenWidth } = Dimensions.get('window');
const itemSize = (screenWidth - 48 - 24) / 3; // 3 columns with margins

const ImageGridItem: React.FC<ImageGridItemProps> = ({ 
  imageUrl, 
  tagCount, 
  isPublished, 
  onPress,
  isSelected = false,
  presetName,
  isSynced = true,
  isEdited = false,
  filename
}) => {
  const scaleAnim = useRef(new Animated.Value(1)).current;

  const handlePress = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    onPress();
  };

  const handlePressIn = () => {
    Animated.spring(scaleAnim, {
      toValue: 0.95,
      useNativeDriver: true,
      tension: 100,
      friction: 8,
    }).start();
  };

  const handlePressOut = () => {
    Animated.spring(scaleAnim, {
      toValue: 1,
      useNativeDriver: true,
      tension: 100,
      friction: 8,
    }).start();
  };

  const getFilename = (url: string) => {
    if (filename) return filename;
    const parts = url.split('/');
    return parts[parts.length - 1].split('?')[0] || 'DSC_0001';
  };

  return (
    <Animated.View style={{ transform: [{ scale: scaleAnim }] }}>
      <TouchableOpacity
        style={[
          styles.container,
          isSelected && styles.selectedContainer
        ]}
        onPress={handlePress}
        onPressIn={handlePressIn}
        onPressOut={handlePressOut}
        activeOpacity={0.9}
        accessibilityRole="button"
        accessibilityLabel={`View image with ${tagCount} tags`}
      >
        <Image
          source={{ uri: imageUrl }}
          style={styles.image}
          resizeMode="cover"
        />
        
        {/* Selection Indicator */}
        {isSelected && (
          <View style={styles.selectionIndicator}>
            <Ionicons name="checkmark-circle" size={24} color={theme.colors.semantic.primary} />
          </View>
        )}
        
        {/* Top Left Overlay - Filename */}
        <View style={styles.filenameOverlay}>
          <Text style={styles.filenameText} numberOfLines={1}>
            {getFilename(imageUrl)}
          </Text>
        </View>
        
        {/* Top Right Overlay - Status Badges */}
        <View style={styles.statusOverlay}>
          {/* Sync Status */}
          <View style={[
            styles.statusBadge,
            isSynced ? styles.syncedBadge : styles.unsyncedBadge
          ]}>
            <Ionicons 
              name={isSynced ? "checkmark-circle" : "sync"} 
              size={12} 
              color={theme.colors.surface} 
            />
          </View>
          
          {/* Preset Badge */}
          {presetName && (
            <View style={styles.presetBadge}>
              <Ionicons name="color-palette" size={12} color={theme.colors.surface} />
            </View>
          )}
          
          {/* Edit Status */}
          {isEdited && (
            <View style={styles.editedBadge}>
              <Ionicons name="create" size={12} color={theme.colors.surface} />
            </View>
          )}
          
          {/* Published Status */}
          {isPublished && (
            <View style={styles.publishedBadge}>
              <Ionicons name="globe" size={12} color={theme.colors.surface} />
            </View>
          )}
        </View>
        
        {/* Bottom Overlay - Tag Count */}
        <View style={styles.bottomOverlay}>
          <View style={styles.tagBadge}>
            <Ionicons name="pricetag" size={12} color={theme.colors.surface} />
            <Text style={styles.tagCount}>{tagCount}</Text>
          </View>
        </View>
      </TouchableOpacity>
    </Animated.View>
  );
};

const styles = StyleSheet.create({
  container: {
    width: itemSize,
    height: itemSize,
    marginBottom: 12,
    borderRadius: 12,
    overflow: 'hidden',
    backgroundColor: theme.colors.surface,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  selectedContainer: {
    borderWidth: 3,
    borderColor: theme.colors.semantic.primary,
  },
  image: {
    width: '100%',
    height: '100%',
  },
  selectionIndicator: {
    position: 'absolute',
    top: 8,
    left: 8,
    backgroundColor: 'rgba(255, 255, 255, 0.95)',
    borderRadius: 12,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.2,
    shadowRadius: 2,
    elevation: 2,
  },
  filenameOverlay: {
    position: 'absolute',
    top: 8,
    left: 8,
    backgroundColor: 'rgba(0, 0, 0, 0.7)',
    paddingHorizontal: 8,
    paddingVertical: 4,
    borderRadius: 6,
    maxWidth: itemSize * 0.6,
  },
  filenameText: {
    fontSize: 10,
    fontWeight: '600',
    color: theme.colors.surface,
  },
  statusOverlay: {
    position: 'absolute',
    top: 8,
    right: 8,
    alignItems: 'flex-end',
    gap: 4,
  },
  statusBadge: {
    width: 20,
    height: 20,
    borderRadius: 10,
    justifyContent: 'center',
    alignItems: 'center',
  },
  syncedBadge: {
    backgroundColor: theme.colors.semantic.success,
  },
  unsyncedBadge: {
    backgroundColor: theme.colors.semantic.warning,
  },
  presetBadge: {
    width: 20,
    height: 20,
    borderRadius: 10,
    backgroundColor: theme.colors.semantic.primary,
    justifyContent: 'center',
    alignItems: 'center',
  },
  editedBadge: {
    width: 20,
    height: 20,
    borderRadius: 10,
    backgroundColor: theme.colors.semantic.info,
    justifyContent: 'center',
    alignItems: 'center',
  },
  publishedBadge: {
    width: 20,
    height: 20,
    borderRadius: 10,
    backgroundColor: theme.colors.semantic.secondary,
    justifyContent: 'center',
    alignItems: 'center',
  },
  bottomOverlay: {
    position: 'absolute',
    bottom: 8,
    left: 8,
    right: 8,
    alignItems: 'flex-start',
  },
  tagBadge: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: 'rgba(0, 0, 0, 0.7)',
    paddingHorizontal: 8,
    paddingVertical: 4,
    borderRadius: 8,
    gap: 4,
  },
  tagCount: {
    fontSize: 12,
    fontWeight: '600',
    color: theme.colors.surface,
  },
});

export default ImageGridItem;
