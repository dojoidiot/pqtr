import React, { useState } from 'react';
import {
  SafeAreaView,
  Text,
  View,
  TouchableOpacity,
  FlatList,
  StyleSheet,
  Image,
  Alert,
  ActionSheetIOS,
  Platform,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';
import { Preset } from '../types';

interface PresetManagerScreenProps {
  navigation: any;
}

const dummyPresets: Preset[] = [
  {
    id: '1',
    name: 'Track Day',
    description: 'High contrast, vibrant racing shots with enhanced saturation',
    thumbnailUri: 'https://images.unsplash.com/photo-1541899481282-d53bffe3c35d?w=150&h=150&fit=crop',
    createdAt: new Date('2024-01-15').toISOString(),
    lastEdited: new Date('2024-01-20').toISOString(),
    version: 2,
    sharedWithTeam: true,
    createdBy: 'James Horne',
    appliedCount: 28,
    isActive: true,
    settings: {
      brightness: 0.1,
      contrast: 0.3,
      saturation: 0.4,
      temperature: 0.2,
      tint: 0.0,
      highlights: -0.2,
      shadows: 0.3,
      sharpness: 0.5,
      vignette: 0.1,
    },
    previousVersions: [
      {
        id: '1_v1',
        version: 1,
        createdAt: new Date('2024-01-15').toISOString(),
        settings: {
          brightness: 0.0,
          contrast: 0.2,
          saturation: 0.3,
          temperature: 0.1,
          tint: 0.0,
          highlights: -0.1,
          shadows: 0.2,
          sharpness: 0.4,
          vignette: 0.0,
        },
        changes: ['Initial version'],
      }
    ],
  },
  {
    id: '2',
    name: 'Neutral Film',
    description: 'Classic film look with natural tones and subtle grain',
    thumbnailUri: 'https://images.unsplash.com/photo-1551698618-1dfe5d97d256?w=150&h=150&fit=crop',
    createdAt: new Date('2024-01-10').toISOString(),
    lastEdited: new Date('2024-01-18').toISOString(),
    version: 1,
    sharedWithTeam: false,
    createdBy: 'James Horne',
    appliedCount: 15,
    isActive: false,
    settings: {
      brightness: 0.0,
      contrast: 0.1,
      saturation: -0.1,
      temperature: 0.0,
      tint: 0.0,
      highlights: 0.0,
      shadows: 0.1,
      sharpness: 0.2,
      vignette: 0.0,
    },
  },
  {
    id: '3',
    name: 'Aston Mono',
    description: 'Monochrome with rich blacks and high contrast',
    thumbnailUri: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=150&h=150&fit=crop',
    createdAt: new Date('2024-01-08').toISOString(),
    lastEdited: new Date('2024-01-16').toISOString(),
    version: 3,
    sharedWithTeam: true,
    createdBy: 'Sarah Chen',
    appliedCount: 42,
    isActive: false,
    settings: {
      brightness: 0.0,
      contrast: 0.4,
      saturation: -1.0,
      temperature: 0.0,
      tint: 0.0,
      highlights: -0.3,
      shadows: 0.5,
      sharpness: 0.6,
      vignette: 0.2,
    },
    previousVersions: [
      {
        id: '3_v1',
        version: 1,
        createdAt: new Date('2024-01-08').toISOString(),
        settings: {
          brightness: 0.0,
          contrast: 0.3,
          saturation: -1.0,
          temperature: 0.0,
          tint: 0.0,
          highlights: -0.2,
          shadows: 0.4,
          sharpness: 0.5,
          vignette: 0.1,
        },
        changes: ['Initial version'],
      },
      {
        id: '3_v2',
        version: 2,
        createdAt: new Date('2024-01-12').toISOString(),
        settings: {
          brightness: 0.0,
          contrast: 0.35,
          saturation: -1.0,
          temperature: 0.0,
          tint: 0.0,
          highlights: -0.25,
          shadows: 0.45,
          sharpness: 0.55,
          vignette: 0.15,
        },
        changes: ['Enhanced contrast and sharpness'],
      }
    ],
  },
  {
    id: '4',
    name: 'Sunset Glow',
    description: 'Warm, golden hour aesthetics with enhanced warmth',
    thumbnailUri: 'https://images.unsplash.com/photo-1506905925346-21bda4d32df4?w=150&h=150&fit=crop',
    createdAt: new Date('2024-01-05').toISOString(),
    lastEdited: new Date('2024-01-14').toISOString(),
    version: 1,
    sharedWithTeam: false,
    createdBy: 'James Horne',
    appliedCount: 19,
    isActive: false,
    settings: {
      brightness: 0.1,
      contrast: 0.1,
      saturation: 0.2,
      temperature: 0.4,
      tint: 0.1,
      highlights: 0.2,
      shadows: 0.1,
      sharpness: 0.3,
      vignette: 0.0,
    },
  },
  {
    id: '5',
    name: 'Urban Edge',
    description: 'Sharp, modern city photography with enhanced clarity',
    thumbnailUri: 'https://images.unsplash.com/photo-1541899481282-d53bffe3c35d?w=150&h=150&fit=crop',
    createdAt: new Date('2024-01-03').toISOString(),
    lastEdited: new Date('2024-01-11').toISOString(),
    version: 2,
    sharedWithTeam: true,
    createdBy: 'Mike Rodriguez',
    appliedCount: 33,
    isActive: false,
    settings: {
      brightness: 0.0,
      contrast: 0.2,
      saturation: 0.1,
      temperature: 0.0,
      tint: 0.0,
      highlights: 0.1,
      shadows: 0.2,
      sharpness: 0.7,
      vignette: 0.1,
    },
    previousVersions: [
      {
        id: '5_v1',
        version: 1,
        createdAt: new Date('2024-01-03').toISOString(),
        settings: {
          brightness: 0.0,
          contrast: 0.15,
          saturation: 0.05,
          temperature: 0.0,
          tint: 0.0,
          highlights: 0.05,
          shadows: 0.15,
          sharpness: 0.6,
          vignette: 0.05,
        },
        changes: ['Initial version'],
      }
    ],
  },
  {
    id: '6',
    name: 'Portrait Pro',
    description: 'Professional portrait enhancement with skin smoothing',
    thumbnailUri: 'https://images.unsplash.com/photo-1551698618-1dfe5d97d256?w=150&h=150&fit=crop',
    createdAt: new Date('2024-01-01').toISOString(),
    lastEdited: new Date('2024-01-09').toISOString(),
    version: 1,
    sharedWithTeam: true,
    createdBy: 'Emma Wilson',
    appliedCount: 67,
    isActive: false,
    settings: {
      brightness: 0.05,
      contrast: 0.1,
      saturation: -0.1,
      temperature: 0.1,
      tint: 0.0,
      highlights: 0.1,
      shadows: 0.2,
      sharpness: 0.4,
      vignette: 0.0,
    },
  },
];

const PresetManagerScreen: React.FC<PresetManagerScreenProps> = ({ navigation }) => {
  const [presets, setPresets] = useState<Preset[]>(dummyPresets);

  const handleCreatePreset = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    navigation.navigate('PresetEditor', { mode: 'create' });
  };

  const handlePresetPress = (preset: Preset) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    
    if (Platform.OS === 'ios') {
      ActionSheetIOS.showActionSheetWithOptions(
        {
          options: ['Cancel', 'Apply Preset', 'Edit Preset', 'Duplicate', 'Delete'],
          cancelButtonIndex: 0,
          destructiveButtonIndex: 4,
        },
        (buttonIndex) => {
          switch (buttonIndex) {
            case 1:
              handleApplyPreset(preset);
              break;
            case 2:
              handleEditPreset(preset);
              break;
            case 3:
              handleDuplicatePreset(preset);
              break;
            case 4:
              handleDeletePreset(preset);
              break;
          }
        }
      );
    } else {
      // For Android, show a simple alert for now
      Alert.alert(
        preset.name,
        'Choose an action:',
        [
          { text: 'Cancel', style: 'cancel' },
          { text: 'Apply', onPress: () => handleApplyPreset(preset) },
          { text: 'Edit', onPress: () => handleEditPreset(preset) },
          { text: 'Duplicate', onPress: () => handleDuplicatePreset(preset) },
          { text: 'Delete', onPress: () => handleDeletePreset(preset), style: 'destructive' },
        ]
      );
    }
  };

  const handleApplyPreset = (preset: Preset) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    
    // Update all presets to set the selected one as active
    setPresets(prevPresets => 
      prevPresets.map(p => ({
        ...p,
        isActive: p.id === preset.id
      }))
    );
    
    Alert.alert('Preset Applied', `"${preset.name}" is now active`);
  };

  const handleEditPreset = (preset: Preset) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    navigation.navigate('PresetEditor', { mode: 'edit', preset });
  };

  const handleDuplicatePreset = (preset: Preset) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    
    const duplicatedPreset: Preset = {
      ...preset,
      id: Date.now().toString(),
      name: `${preset.name} (Copy)`,
      createdAt: new Date().toISOString(),
      lastEdited: new Date().toISOString(),
      version: 1,
      appliedCount: 0,
      isActive: false,
      sharedWithTeam: false,
    };
    
    setPresets(prevPresets => [duplicatedPreset, ...prevPresets]);
    Alert.alert('Preset Duplicated', `"${duplicatedPreset.name}" has been created`);
  };

  const handleDeletePreset = (preset: Preset) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Heavy);
    
    Alert.alert(
      'Delete Preset',
      `Are you sure you want to delete "${preset.name}"? This action cannot be undone.`,
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Delete',
          style: 'destructive',
          onPress: () => {
            setPresets(prevPresets => prevPresets.filter(p => p.id !== preset.id));
            Alert.alert('Deleted', `"${preset.name}" has been removed`);
          },
        },
      ]
    );
  };

  const renderPresetItem = ({ item }: { item: Preset }) => (
    <TouchableOpacity
      style={styles.presetCard}
      onPress={() => handlePresetPress(item)}
      activeOpacity={0.7}
      accessibilityRole="button"
      accessibilityLabel={`${item.name} preset, ${item.description}`}
    >
      {/* Thumbnail */}
      <Image source={{ uri: item.thumbnailUri }} style={styles.thumbnail} />
      
      {/* Content */}
      <View style={styles.presetContent}>
        <View style={styles.presetHeader}>
          <View style={styles.presetTitleRow}>
            <Text style={styles.presetName} numberOfLines={1}>
              {item.name}
            </Text>
            {item.isActive && (
              <View style={styles.activeBadge}>
                <Text style={styles.activeBadgeText}>Active</Text>
              </View>
            )}
          </View>
          
          {/* Team sharing indicator */}
          {item.sharedWithTeam && (
            <View style={styles.teamIndicator}>
              <Ionicons name="people" size={14} color={theme.colors.status.success} />
              <Text style={styles.teamText}>Team</Text>
            </View>
          )}
        </View>
        
        <Text style={styles.presetDescription} numberOfLines={2}>
          {item.description}
        </Text>
        
        <View style={styles.presetFooter}>
          <View style={styles.presetInfo}>
            <Text style={styles.createdDate}>
              Version {item.version} • {new Date(item.lastEdited).toLocaleDateString()}
            </Text>
            <Text style={styles.createdBy}>
              by {item.createdBy}
            </Text>
          </View>
          <Text style={styles.appliedCount}>
            Used in {item.appliedCount} photo{item.appliedCount !== 1 ? 's' : ''}
          </Text>
        </View>
      </View>
      
      {/* Chevron */}
      <View style={styles.chevronContainer}>
        <Ionicons name="chevron-forward" size={20} color={theme.colors.text.tertiary} />
      </View>
    </TouchableOpacity>
  );

  const renderSectionHeader = (title: string, count: number) => (
    <View style={styles.sectionHeader}>
      <Text style={styles.sectionTitle}>{title}</Text>
      <Text style={styles.sectionCount}>{count}</Text>
    </View>
  );

  const teamPresets = presets.filter(p => p.sharedWithTeam);
  const personalPresets = presets.filter(p => !p.sharedWithTeam);

  const renderPresetSection = (presets: Preset[], title: string) => {
    if (presets.length === 0) return null;
    
    return (
      <View style={styles.section}>
        {renderSectionHeader(title, presets.length)}
        {presets.map((preset) => (
          <View key={preset.id}>
            {renderPresetItem({ item: preset })}
            <View style={styles.separator} />
          </View>
        ))}
      </View>
    );
  };

  return (
    <SafeAreaView style={styles.container}>
      {/* Top Bar */}
      <View style={styles.header}>
        <TouchableOpacity
          style={styles.backButton}
          onPress={() => navigation.goBack()}
          accessibilityLabel="Go back"
          accessibilityRole="button"
        >
          <Ionicons name="chevron-back" size={24} color={theme.colors.text.primary} />
        </TouchableOpacity>
        
        <Text style={styles.headerTitle}>Presets</Text>
        
        <TouchableOpacity
          style={styles.createButton}
          onPress={handleCreatePreset}
          accessibilityLabel="Create new preset"
          accessibilityRole="button"
        >
          <Ionicons name="add" size={24} color={theme.colors.accent} />
        </TouchableOpacity>
      </View>

      {/* Preset List */}
      <FlatList
        data={[]} // Empty data since we're rendering sections manually
        renderItem={() => null}
        ListHeaderComponent={() => (
          <View style={styles.presetList}>
            {renderPresetSection(teamPresets, 'Team Presets')}
            {renderPresetSection(personalPresets, 'My Presets')}
          </View>
        )}
        showsVerticalScrollIndicator={false}
        contentContainerStyle={styles.presetList}
      />

      {/* Floating Action Button */}
      <TouchableOpacity
        style={styles.fab}
        onPress={handleCreatePreset}
        accessibilityLabel="Create new preset"
        accessibilityRole="button"
      >
        <Ionicons name="add" size={24} color={theme.colors.surface} />
      </TouchableOpacity>
    </SafeAreaView>
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
  },
  backButton: {
    padding: theme.spacing.s,
    borderRadius: theme.borderRadius.s,
    backgroundColor: theme.colors.lightGray,
  },
  headerTitle: {
    fontSize: theme.fontSizes.title,
    fontWeight: '700',
    color: theme.colors.text.primary,
  },
  createButton: {
    padding: theme.spacing.s,
    borderRadius: theme.borderRadius.s,
    backgroundColor: theme.colors.lightGray,
  },
  presetList: {
    padding: theme.spacing.l,
    paddingBottom: 100,
  },
  section: {
    marginBottom: theme.spacing.xl,
  },
  sectionHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: theme.spacing.m,
  },
  sectionTitle: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
  sectionCount: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
    fontWeight: '500',
  },
  presetCard: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.m,
    padding: theme.spacing.m,
    borderWidth: 1,
    borderColor: theme.colors.border,
    ...theme.shadows.subtle,
  },
  thumbnail: {
    width: 60,
    height: 60,
    borderRadius: theme.borderRadius.s,
    marginRight: theme.spacing.m,
  },
  presetContent: {
    flex: 1,
  },
  presetHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'flex-start',
    marginBottom: theme.spacing.xs,
  },
  presetTitleRow: {
    flexDirection: 'row',
    alignItems: 'center',
    flex: 1,
    marginRight: theme.spacing.s,
  },
  presetName: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '700',
    color: theme.colors.text.primary,
    flex: 1,
    marginRight: theme.spacing.s,
  },
  activeBadge: {
    backgroundColor: theme.colors.status.success,
    paddingHorizontal: theme.spacing.s,
    paddingVertical: 4,
    borderRadius: theme.borderRadius.s,
  },
  activeBadgeText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.surface,
  },
  teamIndicator: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: 'rgba(52, 199, 89, 0.1)',
    paddingHorizontal: theme.spacing.s,
    paddingVertical: 4,
    borderRadius: theme.borderRadius.s,
    gap: 4,
  },
  teamText: {
    fontSize: theme.fontSizes.caption,
    fontWeight: '600',
    color: theme.colors.status.success,
  },
  presetDescription: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.secondary,
    lineHeight: 18,
    marginBottom: theme.spacing.s,
  },
  presetFooter: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  presetInfo: {
    flex: 1,
  },
  createdDate: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.tertiary,
    fontWeight: '500',
    marginBottom: 2,
  },
  createdBy: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
    fontWeight: '400',
  },
  appliedCount: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
    fontWeight: '600',
  },
  chevronContainer: {
    marginLeft: theme.spacing.s,
  },
  separator: {
    height: theme.spacing.m,
  },
  fab: {
    position: 'absolute',
    bottom: 100,
    right: theme.spacing.l,
    width: 56,
    height: 56,
    borderRadius: 28,
    backgroundColor: theme.colors.accent,
    justifyContent: 'center',
    alignItems: 'center',
    ...theme.shadows.medium,
  },
});

export default PresetManagerScreen;
