import React, { useState, useRef } from 'react';
import {
  SafeAreaView,
  Text,
  View,
  TouchableOpacity,
  StyleSheet,
  ScrollView,
  TextInput,
  Alert,
  Animated,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';
import BackButton from '../components/BackButton';
import { Preset, PresetSettings } from '../types';

interface PresetEditorScreenProps {
  navigation: any;
  route: any;
}

const PresetEditorScreen: React.FC<PresetEditorScreenProps> = ({ navigation, route }) => {
  const { mode, preset } = route.params || {};
  
  const [presetName, setPresetName] = useState(preset?.name || '');
  const [presetDescription, setPresetDescription] = useState(preset?.description || '');
  const [settings, setSettings] = useState<PresetSettings>(preset?.settings || {
    brightness: 0,
    contrast: 0,
    saturation: 0,
    temperature: 0,
    tint: 0,
    highlights: 0,
    shadows: 0,
    sharpness: 0,
    vignette: 0,
  });

  const fadeAnim = useRef(new Animated.Value(0)).current;
  const slideAnim = useRef(new Animated.Value(20)).current;

  React.useEffect(() => {
    Animated.parallel([
      Animated.timing(fadeAnim, {
        toValue: 1,
        duration: 600,
        useNativeDriver: true,
      }),
      Animated.timing(slideAnim, {
        toValue: 0,
        duration: 600,
        useNativeDriver: true,
      }),
    ]).start();
  }, []);

  const handleSettingChange = (setting: keyof PresetSettings, value: number) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setSettings(prev => ({
      ...prev,
      [setting]: value,
    }));
  };

  const handleSave = () => {
    if (!presetName.trim()) {
      Alert.alert('Missing Name', 'Please enter a preset name.');
      return;
    }

    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    
    const presetData = {
      id: preset?.id || Date.now().toString(),
      name: presetName.trim(),
      description: presetDescription.trim(),
      settings,
      createdAt: preset?.createdAt || new Date().toISOString(),
      lastEdited: new Date().toISOString(),
      version: (preset?.version || 0) + 1,
    };

    console.log('Saving preset:', presetData);
    Alert.alert('Success', 'Preset saved successfully!', [
      { text: 'OK', onPress: () => navigation.goBack() }
    ]);
  };

  const renderSlider = (
    setting: keyof PresetSettings,
    label: string,
    min: number = -100,
    max: number = 100,
    step: number = 1
  ) => {
    const value = settings[setting];
    const percentage = ((value - min) / (max - min)) * 100;

    return (
      <View style={styles.sliderContainer}>
        <View style={styles.sliderHeader}>
          <Text style={styles.sliderLabel}>{label}</Text>
          <Text style={styles.sliderValue}>{value}</Text>
        </View>
        
        <View style={styles.sliderTrack}>
          <View style={[styles.sliderFill, { width: `${percentage}%` }]} />
          <View style={styles.sliderThumb} />
        </View>
        
        <View style={styles.sliderControls}>
          <TouchableOpacity
            style={styles.sliderButton}
            onPress={() => handleSettingChange(setting, Math.max(min, value - step))}
          >
            <Ionicons name="remove" size={16} color={theme.colors.text.secondary} />
          </TouchableOpacity>
          
          <TouchableOpacity
            style={styles.sliderButton}
            onPress={() => handleSettingChange(setting, 0)}
          >
            <Text style={styles.resetButtonText}>Reset</Text>
          </TouchableOpacity>
          
          <TouchableOpacity
            style={styles.sliderButton}
            onPress={() => handleSettingChange(setting, Math.min(max, value + step))}
          >
            <Ionicons name="add" size={16} color={theme.colors.text.secondary} />
          </TouchableOpacity>
        </View>
      </View>
    );
  };

  return (
    <SafeAreaView style={styles.container}>
      {/* Header with Back Button */}
      <View style={styles.header}>
        <BackButton onPress={() => navigation.goBack()} />
        <View style={styles.headerContent}>
          <Text style={styles.headerTitle}>
            {mode === 'create' ? 'Create Preset' : 'Edit Preset'}
          </Text>
          <Text style={styles.headerSubtitle}>
            {mode === 'create' ? 'Build a new editing preset' : `Editing: ${preset?.name || 'Preset'}`}
          </Text>
        </View>
        
        <TouchableOpacity
          style={styles.saveButton}
          onPress={handleSave}
        >
          <Text style={styles.saveButtonText}>Save</Text>
        </TouchableOpacity>
      </View>

      <Animated.View 
        style={[
          styles.content,
          {
            opacity: fadeAnim,
            transform: [{ translateY: slideAnim }],
          }
        ]}
      >
        <ScrollView style={styles.scrollContent} showsVerticalScrollIndicator={false}>
          {/* Basic Info */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Basic Information</Text>
            
            <View style={styles.inputContainer}>
              <Text style={styles.inputLabel}>Preset Name</Text>
              <TextInput
                style={styles.textInput}
                placeholder="Enter preset name"
                placeholderTextColor={theme.colors.text.tertiary}
                value={presetName}
                onChangeText={setPresetName}
              />
            </View>
            
            <View style={styles.inputContainer}>
              <Text style={styles.inputLabel}>Description</Text>
              <TextInput
                style={[styles.textInput, styles.textArea]}
                placeholder="Describe what this preset does"
                placeholderTextColor={theme.colors.text.tertiary}
                value={presetDescription}
                onChangeText={setPresetDescription}
                multiline
                numberOfLines={3}
              />
            </View>
          </View>

          {/* Exposure Settings */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Exposure</Text>
            <Text style={styles.sectionSubtitle}>Adjust brightness and contrast</Text>
            
            {renderSlider('brightness', 'Brightness', -100, 100)}
            {renderSlider('contrast', 'Contrast', -100, 100)}
            {renderSlider('highlights', 'Highlights', -100, 100)}
            {renderSlider('shadows', 'Shadows', -100, 100)}
          </View>

          {/* Color Settings */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Color</Text>
            <Text style={styles.sectionSubtitle}>Fine-tune color adjustments</Text>
            
            {renderSlider('saturation', 'Saturation', -100, 100)}
            {renderSlider('temperature', 'Temperature', -100, 100)}
            {renderSlider('tint', 'Tint', -100, 100)}
          </View>

          {/* Detail Settings */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Detail</Text>
            <Text style={styles.sectionSubtitle}>Enhance image details</Text>
            
            {renderSlider('sharpness', 'Sharpness', 0, 100)}
            {renderSlider('vignette', 'Vignette', -100, 100)}
          </View>

          {/* Preset Preview */}
          <View style={styles.section}>
            <Text style={styles.sectionTitle}>Preview</Text>
            <Text style={styles.sectionSubtitle}>See how your settings look</Text>
            
            <View style={styles.previewContainer}>
              <View style={styles.previewImage}>
                <Ionicons name="image-outline" size={48} color={theme.colors.text.tertiary} />
                <Text style={styles.previewText}>Preview Image</Text>
              </View>
              
              <View style={styles.previewInfo}>
                <Text style={styles.previewLabel}>Current Settings:</Text>
                <Text style={styles.previewSettings}>
                  Brightness: {settings.brightness} • Contrast: {settings.contrast} • Saturation: {settings.saturation}
                </Text>
                <Text style={styles.previewSettings}>
                  Temperature: {settings.temperature} • Sharpness: {settings.sharpness}
                </Text>
              </View>
            </View>
          </View>

          <View style={styles.bottomSpacing} />
        </ScrollView>
      </Animated.View>
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
    paddingTop: theme.spacing.s,
    paddingBottom: theme.spacing.m,
    paddingHorizontal: theme.spacing.l,
    borderBottomWidth: 1,
    borderBottomColor: theme.colors.border,
  },
  headerContent: {
    flex: 1,
    marginLeft: theme.spacing.m,
  },
  headerTitle: {
    fontSize: theme.fontSizes.title1,
    fontWeight: '700',
    color: theme.colors.text.primary,
    letterSpacing: -0.5,
    marginBottom: theme.spacing.xs,
  },
  headerSubtitle: {
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.secondary,
    fontWeight: '400',
  },
  saveButton: {
    backgroundColor: theme.colors.green,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.s,
    borderRadius: theme.borderRadius.m,
  },
  saveButtonText: {
    color: theme.colors.surface,
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
  },
  content: {
    flex: 1,
  },
  scrollContent: {
    flex: 1,
    paddingHorizontal: theme.spacing.l,
  },
  section: {
    marginBottom: theme.spacing.xl,
  },
  sectionTitle: {
    fontSize: theme.fontSizes.headline,
    fontWeight: '600',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.s,
  },
  sectionSubtitle: {
    fontSize: theme.fontSizes.footnote,
    color: theme.colors.text.secondary,
    marginBottom: theme.spacing.m,
    fontStyle: 'italic',
  },
  inputContainer: {
    marginBottom: theme.spacing.m,
  },
  inputLabel: {
    fontSize: theme.fontSizes.body,
    fontWeight: '500',
    color: theme.colors.text.primary,
    marginBottom: theme.spacing.s,
  },
  textInput: {
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.m,
    paddingHorizontal: theme.spacing.m,
    paddingVertical: theme.spacing.m,
    fontSize: theme.fontSizes.body,
    color: theme.colors.text.primary,
    borderWidth: 1,
    borderColor: theme.colors.border,
    ...theme.shadows.subtle,
  },
  textArea: {
    height: 80,
    textAlignVertical: 'top',
  },
  sliderContainer: {
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.m,
    padding: theme.spacing.m,
    marginBottom: theme.spacing.m,
    borderWidth: 1,
    borderColor: theme.colors.border,
    ...theme.shadows.subtle,
  },
  sliderHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: theme.spacing.m,
  },
  sliderLabel: {
    fontSize: theme.fontSizes.body,
    fontWeight: '500',
    color: theme.colors.text.primary,
  },
  sliderValue: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.green,
    minWidth: 40,
    textAlign: 'right',
  },
  sliderTrack: {
    height: 4,
    backgroundColor: theme.colors.lightGray,
    borderRadius: 2,
    marginBottom: theme.spacing.m,
    position: 'relative',
  },
  sliderFill: {
    height: '100%',
    backgroundColor: theme.colors.green,
    borderRadius: 2,
  },
  sliderThumb: {
    position: 'absolute',
    right: 0,
    top: -4,
    width: 12,
    height: 12,
    backgroundColor: theme.colors.green,
    borderRadius: 6,
    borderWidth: 2,
    borderColor: theme.colors.surface,
  },
  sliderControls: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  sliderButton: {
    padding: theme.spacing.s,
    borderRadius: theme.borderRadius.s,
    backgroundColor: theme.colors.lightGray,
    minWidth: 60,
    alignItems: 'center',
  },
  resetButtonText: {
    fontSize: theme.fontSizes.footnote,
    color: theme.colors.text.secondary,
    fontWeight: '500',
  },
  previewContainer: {
    backgroundColor: theme.colors.surface,
    borderRadius: theme.borderRadius.m,
    padding: theme.spacing.m,
    borderWidth: 1,
    borderColor: theme.colors.border,
    ...theme.shadows.subtle,
  },
  previewImage: {
    height: 120,
    backgroundColor: theme.colors.lightGray,
    borderRadius: theme.borderRadius.m,
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: theme.spacing.m,
  },
  previewText: {
    fontSize: theme.fontSizes.footnote,
    color: theme.colors.text.secondary,
    marginTop: theme.spacing.s,
  },
  previewInfo: {
    gap: theme.spacing.xs,
  },
  previewLabel: {
    fontSize: theme.fontSizes.body,
    fontWeight: '600',
    color: theme.colors.text.primary,
  },
  previewSettings: {
    fontSize: theme.fontSizes.footnote,
    color: theme.colors.text.secondary,
    lineHeight: 18,
  },
  bottomSpacing: {
    height: theme.spacing.xxl,
  },
});

export default PresetEditorScreen;
