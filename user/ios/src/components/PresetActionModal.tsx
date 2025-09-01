import React, { useState } from 'react';
import { 
  View, 
  Text, 
  TouchableOpacity, 
  StyleSheet, 
  Modal,
  Alert
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

interface PresetAction {
  id: string;
  title: string;
  subtitle: string;
  icon: string;
  action: () => void;
}

interface PresetActionModalProps {
  visible: boolean;
  onClose: () => void;
  selectedCount: number;
  totalCount: number;
  onApplyToSelected: () => void;
  onApplyToAll: () => void;
  onApplyToNew: () => void;
}

const PresetActionModal: React.FC<PresetActionModalProps> = ({
  visible,
  onClose,
  selectedCount,
  totalCount,
  onApplyToSelected,
  onApplyToAll,
  onApplyToNew
}) => {
  const [selectedAction, setSelectedAction] = useState<string | null>(null);

  const presetActions: PresetAction[] = [
    {
      id: 'selected',
      title: 'Selected Images',
      subtitle: `${selectedCount} image${selectedCount !== 1 ? 's' : ''} selected`,
      icon: 'checkmark-circle',
      action: () => {
        Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
        setSelectedAction('selected');
        onApplyToSelected();
        onClose();
      }
    },
    {
      id: 'all',
      title: 'All Images',
      subtitle: `${totalCount} total images in project`,
      icon: 'images',
      action: () => {
        Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
        setSelectedAction('all');
        onApplyToAll();
        onClose();
      }
    },
    {
      id: 'new',
      title: 'New Incoming Photos',
      subtitle: 'Auto-apply to future uploads',
      icon: 'cloud-upload',
      action: () => {
        Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
        setSelectedAction('new');
        onApplyToNew();
        onClose();
      }
    }
  ];

  const handleActionPress = (action: PresetAction) => {
    if (action.id === 'selected' && selectedCount === 0) {
      Alert.alert('No Images Selected', 'Please select some images first.');
      return;
    }
    
    if (action.id === 'all' && totalCount === 0) {
      Alert.alert('No Images', 'There are no images in this project.');
      return;
    }
    
    action.action();
  };

  return (
    <Modal
      visible={visible}
      animationType="slide"
      presentationStyle="pageSheet"
      onRequestClose={onClose}
    >
      <View style={styles.container}>
        {/* Header */}
        <View style={styles.header}>
          <TouchableOpacity onPress={onClose} style={styles.closeButton}>
            <Ionicons name="close" size={24} color={theme.colors.text.primary} />
          </TouchableOpacity>
          <Text style={styles.title}>Apply Preset To...</Text>
          <View style={styles.placeholder} />
        </View>

        {/* Content */}
        <View style={styles.content}>
          <View style={styles.description}>
            <Ionicons name="color-palette" size={24} color={theme.colors.semantic.primary} />
            <Text style={styles.descriptionText}>
              Choose where to apply the Track Day preset
            </Text>
          </View>

          <View style={styles.actionsContainer}>
            {presetActions.map((action) => (
              <TouchableOpacity
                key={action.id}
                style={[
                  styles.actionButton,
                  selectedAction === action.id && styles.actionButtonSelected
                ]}
                onPress={() => handleActionPress(action)}
                activeOpacity={0.8}
                disabled={action.id === 'selected' && selectedCount === 0}
              >
                <View style={styles.actionLeft}>
                  <View style={[
                    styles.actionIcon,
                    selectedAction === action.id && styles.actionIconSelected
                  ]}>
                    <Ionicons 
                      name={action.icon as any} 
                      size={20} 
                      color={selectedAction === action.id ? theme.colors.surface : theme.colors.semantic.primary} 
                    />
                  </View>
                  <View style={styles.actionInfo}>
                    <Text style={[
                      styles.actionTitle,
                      selectedAction === action.id && styles.actionTitleSelected
                    ]}>
                      {action.title}
                    </Text>
                    <Text style={[
                      styles.actionSubtitle,
                      selectedAction === action.id && styles.actionSubtitleSelected
                    ]}>
                      {action.subtitle}
                    </Text>
                  </View>
                </View>
                
                <Ionicons 
                  name="chevron-forward" 
                  size={20} 
                  color={theme.colors.text.tertiary} 
                />
              </TouchableOpacity>
            ))}
          </View>

          {/* Footer Info */}
          <View style={styles.footer}>
            <Text style={styles.footerText}>
              Presets are applied non-destructively and can be adjusted later
            </Text>
          </View>
        </View>
      </View>
    </Modal>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: theme.colors.background,
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: theme.spacing.l,
    paddingVertical: theme.spacing.m,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
    backgroundColor: theme.colors.surface,
  },
  closeButton: {
    padding: theme.spacing.s,
  },
  title: {
    fontSize: 18,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
  placeholder: {
    width: 40,
  },
  content: {
    flex: 1,
    padding: theme.spacing.l,
  },
  description: {
    alignItems: 'center',
    marginBottom: theme.spacing.xl,
    gap: theme.spacing.s,
  },
  descriptionText: {
    fontSize: 16,
    color: theme.colors.text.secondary,
    textAlign: 'center',
    lineHeight: 22,
  },
  actionsContainer: {
    gap: theme.spacing.m,
  },
  actionButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    backgroundColor: theme.colors.surface,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.m,
    borderRadius: theme.borderRadius.m,
    borderWidth: 1,
    borderColor: theme.colors.border,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.05,
    shadowRadius: 2,
    elevation: 1,
  },
  actionButtonSelected: {
    backgroundColor: theme.colors.semantic.primary,
    borderColor: theme.colors.semantic.primary,
  },
  actionLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: theme.spacing.m,
    flex: 1,
  },
  actionIcon: {
    width: 44,
    height: 44,
    borderRadius: 22,
    backgroundColor: 'rgba(0, 122, 255, 0.1)',
    justifyContent: 'center',
    alignItems: 'center',
  },
  actionIconSelected: {
    backgroundColor: 'rgba(255, 255, 255, 0.2)',
  },
  actionInfo: {
    flex: 1,
  },
  actionTitle: {
    fontSize: 16,
    fontWeight: '600',
    color: theme.colors.text.primary,
    marginBottom: 2,
  },
  actionTitleSelected: {
    color: theme.colors.surface,
  },
  actionSubtitle: {
    fontSize: 14,
    color: theme.colors.text.secondary,
  },
  actionSubtitleSelected: {
    color: 'rgba(255, 255, 255, 0.8)',
  },
  footer: {
    marginTop: 'auto',
    paddingTop: theme.spacing.l,
    alignItems: 'center',
  },
  footerText: {
    fontSize: 13,
    color: theme.colors.text.tertiary,
    textAlign: 'center',
    lineHeight: 18,
  },
});

export default PresetActionModal;
