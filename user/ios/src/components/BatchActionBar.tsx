import React, { useEffect, useRef } from 'react';
import {
  View,
  Text,
  TouchableOpacity,
  StyleSheet,
  Animated,
  Dimensions,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

const { width: screenWidth } = Dimensions.get('window');

interface BatchActionBarProps {
  isVisible: boolean;
  selectedCount: number;
  onApplyPreset: () => void;
  onTag: () => void;
  onExport: () => void;
  onDelete: () => void;
  onSelectAll: () => void;
  onClearSelection: () => void;
  totalCount: number;
  isAllSelected: boolean;
}

const BatchActionBar: React.FC<BatchActionBarProps> = ({
  isVisible,
  selectedCount,
  onApplyPreset,
  onTag,
  onExport,
  onDelete,
  onSelectAll,
  onClearSelection,
  totalCount,
  isAllSelected,
}) => {
  const slideAnim = useRef(new Animated.Value(100)).current;
  const fadeAnim = useRef(new Animated.Value(0)).current;
  const scaleAnim = useRef(new Animated.Value(0.8)).current;

  useEffect(() => {
    if (isVisible) {
      // Slide up animation
      Animated.spring(slideAnim, {
        toValue: 0,
        useNativeDriver: true,
        tension: 100,
        friction: 8,
      }).start();

      // Fade in backdrop
      Animated.timing(fadeAnim, {
        toValue: 1,
        duration: 200,
        useNativeDriver: true,
      }).start();

      // Scale in content
      Animated.spring(scaleAnim, {
        toValue: 1,
        useNativeDriver: true,
        tension: 100,
        friction: 8,
      }).start();
    } else {
      // Slide down animation
      Animated.spring(slideAnim, {
        toValue: 100,
        useNativeDriver: true,
        tension: 100,
        friction: 8,
      }).start();

      // Fade out backdrop
      Animated.timing(fadeAnim, {
        toValue: 0,
        duration: 200,
        useNativeDriver: true,
      }).start();

      // Scale out content
      Animated.spring(scaleAnim, {
        toValue: 0.8,
        useNativeDriver: true,
        tension: 100,
        friction: 8,
      }).start();
    }
  }, [isVisible]);

  const handleAction = (action: () => void) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    action();
  };

  const handleSelectToggle = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    if (isAllSelected) {
      onClearSelection();
    } else {
      onSelectAll();
    }
  };

  if (!isVisible) return null;

  return (
    <>
      {/* Backdrop */}
      <Animated.View 
        style={[
          styles.backdrop,
          { opacity: fadeAnim }
        ]} 
      />
      
      {/* Action Bar */}
      <Animated.View 
        style={[
          styles.container,
          {
            transform: [
              { translateY: slideAnim },
              { scale: scaleAnim }
            ]
          }
        ]}
      >
        {/* Selection Info */}
        <View style={styles.selectionInfo}>
          <TouchableOpacity 
            style={styles.selectionToggle}
            onPress={handleSelectToggle}
          >
            <Ionicons 
              name={isAllSelected ? "checkbox" : "square-outline"} 
              size={20} 
              color={isAllSelected ? theme.colors.green : theme.colors.text.secondary} 
            />
            <Text style={styles.selectionText}>
              {selectedCount} of {totalCount} selected
            </Text>
          </TouchableOpacity>
          
          <TouchableOpacity 
            style={styles.clearButton}
            onPress={onClearSelection}
          >
            <Text style={styles.clearButtonText}>Clear</Text>
          </TouchableOpacity>
        </View>

        {/* Action Buttons */}
        <View style={styles.actions}>
          {/* Apply Preset */}
          <TouchableOpacity 
            style={styles.actionButton}
            onPress={() => handleAction(onApplyPreset)}
          >
            <View style={styles.actionIconContainer}>
              <Ionicons name="brush-outline" size={20} color={theme.colors.system.orange} />
            </View>
            <Text style={styles.actionText}>Preset</Text>
          </TouchableOpacity>

          {/* Tag */}
          <TouchableOpacity 
            style={styles.actionButton}
            onPress={() => handleAction(onTag)}
          >
            <View style={styles.actionIconContainer}>
              <Ionicons name="pricetag-outline" size={20} color={theme.colors.system.blue} />
            </View>
            <Text style={styles.actionText}>Tag</Text>
          </TouchableOpacity>

          {/* Export */}
          <TouchableOpacity 
            style={styles.actionButton}
            onPress={() => handleAction(onExport)}
          >
            <View style={styles.actionIconContainer}>
              <Ionicons name="download-outline" size={20} color={theme.colors.system.green} />
            </View>
            <Text style={styles.actionText}>Export</Text>
          </TouchableOpacity>

          {/* Delete */}
          <TouchableOpacity 
            style={styles.actionButton}
            onPress={() => handleAction(onDelete)}
          >
            <View style={[styles.actionIconContainer, styles.deleteIconContainer]}>
              <Ionicons name="trash-outline" size={20} color={theme.colors.system.red} />
            </View>
            <Text style={[styles.actionText, styles.deleteText]}>Delete</Text>
          </TouchableOpacity>
        </View>

        {/* Quick Actions */}
        <View style={styles.quickActions}>
          <TouchableOpacity 
            style={styles.quickActionButton}
            onPress={() => handleAction(onApplyPreset)}
          >
            <Ionicons name="flash-outline" size={16} color={theme.colors.text.secondary} />
            <Text style={styles.quickActionText}>Quick Preset</Text>
          </TouchableOpacity>
          
          <TouchableOpacity 
            style={styles.quickActionButton}
            onPress={() => handleAction(onExport)}
          >
            <Ionicons name="share-outline" size={16} color={theme.colors.text.secondary} />
            <Text style={styles.quickActionText}>Share</Text>
          </TouchableOpacity>
        </View>
      </Animated.View>
    </>
  );
};

const styles = StyleSheet.create({
  backdrop: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: 'rgba(0, 0, 0, 0.3)',
    zIndex: 999,
  },
  container: {
    position: 'absolute',
    bottom: 0,
    left: 0,
    right: 0,
    backgroundColor: theme.colors.surface,
    borderTopLeftRadius: theme.borderRadius.xl,
    borderTopRightRadius: theme.borderRadius.xl,
    paddingTop: theme.spacing.l,
    paddingBottom: theme.spacing.l + 34, // Extra padding for safe area
    ...theme.shadows.large,
    zIndex: 1000,
  },
  selectionInfo: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.l,
    marginBottom: theme.spacing.l,
  },
  selectionToggle: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.s,
  },
  selectionText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
  clearButton: {
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
  },
  clearButtonText: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
  actions: {
    flexDirection: 'row',
    justifyContent: 'space-around',
    paddingHorizontal: theme.spacing.l,
    marginBottom: theme.spacing.l,
  },
  actionButton: {
    alignItems: 'center',
    gap: theme.spacing.xs,
    flex: 1,
  },
  actionIconContainer: {
    width: 48,
    height: 48,
    borderRadius: 24,
    backgroundColor: theme.colors.lightGray,
    justifyContent: 'center',
    alignItems: 'center',
    ...theme.shadows.subtle,
  },
  deleteIconContainer: {
    backgroundColor: theme.colors.system.red + '10',
  },
  actionText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.primary,
    textAlign: 'center',
  },
  deleteText: {
    color: theme.colors.system.red,
  },
  quickActions: {
    flexDirection: 'row',
    justifyContent: 'space-around',
    paddingHorizontal: theme.spacing.l,
    paddingTop: theme.spacing.m,
    borderTopWidth: 1,
    borderTopColor: theme.colors.border,
  },
  quickActionButton: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: theme.spacing.s,
    paddingHorizontal: theme.spacing.m,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    gap: theme.spacing.xs,
  },
  quickActionText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.text.secondary,
  },
});

export default BatchActionBar;
