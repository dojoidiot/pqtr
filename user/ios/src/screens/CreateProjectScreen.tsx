// CreateProjectScreen.tsx
// PQTR App — Create New Project Flow (iOS-first, Expo Go compatible)
// Minimalist, elegant, mobile-native design
// Animations: LayoutAnimation + subtle transitions only
// Style tokens: soft beige background, dark green primary, medium rounded edges

import React, { useState } from 'react';
import {
  View,
  Text,
  TextInput,
  ScrollView,
  Switch,
  LayoutAnimation,
  TouchableOpacity,
  UIManager,
  Platform,
  StyleSheet
} from 'react-native';
import { useNavigation } from '@react-navigation/native';
import CameraPicker from '../components/CameraPicker';
import PresetSelector from '../components/PresetSelector';
import ExportOptions from '../components/ExportOptions';
import GradientButton from '../components/GradientButton';

// Enable animation for Android
if (Platform.OS === 'android' && UIManager.setLayoutAnimationEnabledExperimental) {
  UIManager.setLayoutAnimationEnabledExperimental(true);
}

export default function CreateProjectScreen() {
  const navigation = useNavigation();
  const [projectName, setProjectName] = useState('');
  const [camera, setCamera] = useState<string | null>(null);
  const [preset, setPreset] = useState<string | null>(null);
  const [exportFormat, setExportFormat] = useState('9x16');
  const [autoExport, setAutoExport] = useState(true);

  const handleToggleExport = () => {
    LayoutAnimation.configureNext(LayoutAnimation.Presets.easeInEaseOut);
    setAutoExport(!autoExport);
  };

  const handleCreate = () => {
    if (!projectName) return;
    // Save new project logic here
    navigation.goBack();
  };

  return (
    <ScrollView contentContainerStyle={styles.container}>
      <Text style={styles.title}>New Project</Text>

      {/* Project Name Input */}
      <TextInput
        style={styles.input}
        placeholder="Project name..."
        placeholderTextColor="#AAA"
        value={projectName}
        onChangeText={setProjectName}
      />

      {/* Camera Picker */}
      <CameraPicker selected={camera} onSelect={setCamera} />

      {/* Preset Selector */}
      <PresetSelector selected={preset} onSelect={setPreset} />

      {/* Export Options */}
      <ExportOptions selected={exportFormat} onSelect={setExportFormat} />

      {/* Auto Export Toggle */}
      <View style={styles.toggleRow}>
        <Text style={styles.label}>Auto export to camera roll</Text>
        <Switch value={autoExport} onValueChange={handleToggleExport} />
      </View>

      {/* Submit Button */}
      <GradientButton title="Shoot" onPress={handleCreate} />
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    backgroundColor: '#F9F6F1',
    padding: 24,
    paddingBottom: 100,
  },
  title: {
    fontSize: 24,
    fontWeight: '600',
    marginBottom: 24,
    color: '#222',
  },
  input: {
    borderRadius: 14,
    backgroundColor: '#fff',
    paddingVertical: 14,
    paddingHorizontal: 18,
    marginBottom: 24,
    fontSize: 16,
    color: '#222',
  },
  toggleRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginTop: 24,
    marginBottom: 48,
  },
  label: {
    fontSize: 16,
    color: '#333',
  },
});
