import React from 'react';
import { 
  View, 
  Text, 
  TouchableOpacity, 
  StyleSheet, 
  Animated 
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

interface SelectionActionBarProps {
  selectedCount: number;
  onApplyPreset: () => void;
  onPublish: () => void;
  onDelete: () => void;
  onClearSelection: () => void;
}

const SelectionActionBar: React.FC<SelectionActionBarProps> = ({
  selectedCount,
  onApplyPreset,
  onPublish,
  onDelete,
  onClearSelection
}) => {
  if (selectedCount === 0) return null;

  return (
    <Animated.View 
      style={styles.container}
    >
      {/* Selection Info */}
      <View style={styles.selectionInfo}>
        <Ionicons name="images" size={20} color={theme.colors.semantic.primary} />
        <Text style={styles.selectionText}>
          {selectedCount} image{selectedCount !== 1 ? 's' : ''} selected
        </Text>
        <TouchableOpacity onPress={onClearSelection} style={styles.clearButton}>
          <Ionicons name="close" size={16} color={theme.colors.text.secondary} />
        </TouchableOpacity>
      </View>

      {/* Action Buttons */}
      <View style={styles.actionButtons}>
        <TouchableOpacity
          style={[styles.actionButton, styles.presetButton]}
          onPress={onApplyPreset}
          activeOpacity={0.8}
        >
          <Ionicons name="color-palette" size={16} color={theme.colors.surface} />
          <Text style={styles.actionButtonText}>Preset</Text>
        </TouchableOpacity>

        <TouchableOpacity
          style={[styles.actionButton, styles.publishButton]}
          onPress={onPublish}
          activeOpacity={0.8}
        >
          <Ionicons name="globe" size={16} color={theme.colors.surface} />
          <Text style={styles.actionButtonText}>Publish</Text>
        </TouchableOpacity>

        <TouchableOpacity
          style={[styles.actionButton, styles.deleteButton]}
          onPress={onDelete}
          activeOpacity={0.8}
        >
          <Ionicons name="trash" size={16} color={theme.colors.surface} />
          <Text style={styles.actionButtonText}>Delete</Text>
        </TouchableOpacity>
      </View>
    </Animated.View>
  );
};

const styles = StyleSheet.create({
  container: {
    position: 'absolute',
    top: 0,
    left: 0,
    right: 0,
    backgroundColor: theme.colors.surface,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 4,
    zIndex: 1000,
  },
  selectionInfo: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: theme.spacing.l,
    paddingVertical: theme.spacing.m,
    backgroundColor: 'rgba(0, 122, 255, 0.1)',
    borderBottomWidth: 1,
    borderBottomColor: 'rgba(0, 122, 255, 0.2)',
  },
  selectionText: {
    fontSize: 16,
    fontWeight: '600',
    color: theme.colors.semantic.primary,
    flex: 1,
    marginLeft: theme.spacing.s,
  },
  clearButton: {
    padding: theme.spacing.s,
  },
  actionButtons: {
    flexDirection: 'row',
    paddingHorizontal: theme.spacing.l,
    paddingVertical: theme.spacing.m,
    gap: theme.spacing.m,
  },
  actionButton: {
    flex: 1,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: theme.spacing.m,
    paddingHorizontal: theme.spacing.m,
    borderRadius: theme.borderRadius.m,
    gap: theme.spacing.s,
    minHeight: 44,
  },
  presetButton: {
    backgroundColor: theme.colors.semantic.primary,
  },
  publishButton: {
    backgroundColor: theme.colors.semantic.success,
  },
  deleteButton: {
    backgroundColor: theme.colors.semantic.error,
  },
  actionButtonText: {
    fontSize: 14,
    fontWeight: '600',
    color: theme.colors.surface,
  },
});

export default SelectionActionBar;
