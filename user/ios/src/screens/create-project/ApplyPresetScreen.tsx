// ----------------------
// STEP 3: Apply Preset
// ----------------------

import React, { useState } from 'react';
import {
  View,
  Text,
  TextInput,
  TouchableOpacity,
  StyleSheet,
  ScrollView,
  SafeAreaView,
  Dimensions,
  Switch,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { useNavigation, useRoute } from '@react-navigation/native';
import { NativeStackNavigationProp } from '@react-navigation/native-stack';
import { RouteProp } from '@react-navigation/native';
import Slider from '@react-native-community/slider';
import HeaderWithBack from '../../components/create-project/HeaderWithBack';
import StepFooter from '../../components/create-project/StepFooter';
import PresetCarousel from '../../components/create-project/PresetCarousel';
import PresetLivePreviewModal from '../../components/create-project/PresetLivePreviewModal';
import { CreateProjectStackParamList } from '../../navigation/CreateProjectNavigator';

type ApplyPresetScreenNavigationProp = NativeStackNavigationProp<
  CreateProjectStackParamList,
  'ApplyPreset'
>;

type ApplyPresetScreenRouteProp = RouteProp<
  CreateProjectStackParamList,
  'ApplyPreset'
>;

const { width: screenWidth } = Dimensions.get('window');

// Preset names from your LRPRESETS folder
const PRESETS = [
  "AMF1_CHIP_APPROVED",
  "AMF1 - 1NEWNAILEDIT",
  "AMF1 - 1NEWER",
  "AMF1 - B_W",
  "AMF1 - 1NEW1",
  "AM - 4SAM",
  "AMF1 - 2023",
  "Garage - nice",
  "Dusk",
  "asfoftish",
  "ThisCouldWork",
  "AMF1 - Ayo this do be kinda fire tho",
  "AMF1 - NEUTRAL CLEAN SOFT",
  "Miami Vibes",
  "AMF1 - Neutral Faded",
  "Soft but grainy teehee 2",
  "Soft but grainy teehee",
  "AMF1 - GRIME",
  "AMLESSAGGRESSIVE",
  "AMCLEAN",
  "AMF1 - ARTSY AF",
  "AMF1 - CLEAN AS FUCK",
  "looksneutraltomehaha",
  "AMF1 - Garage Preset",
  "AMF1 - PRESET 3 NO CURVES",
  "AMF1 - JPEG",
  "AMF1 - Preset 2",
  "AMF1 - Preset 1",
  "FLADDOCKCLUB",
  "FLAT AS FUCK",
  "Greenedout",
  "Hope Its clean",
  "InstagrammyCars",
  "Its green we gooood",
  "NEWSPLITGOODSHIT",
  "Noblu",
  "PADDOCK - NEUTRAL_",
  "PADDOCK",
  "PinkHighlighterYpo",
  "SEEMS ALRIGHT WFADE",
  "SEEMS ALRIGHT W_COLOUR",
  "SVJ SPECIAL",
  "TEXTURED VINTAGE",
  "WE COOOOL",
  "banegredtion",
  "bangernicercolopurs",
  "basic bitch IG",
  "beachedas(lessfade)",
  "beachedas",
  "cars-icywhite",
  "carsicy-lessfadetho",
  "carsicydehaaazed",
  "carsicywithsomemids",
  "carsnewsplit",
  "carstagramnoblues",
  "cityvibes",
  "cliffvibe",
  "coolio desat",
  "darkicy",
  "f1ex",
  "hellayellow",
  "howsthatfadetho",
  "iceicebaby",
  "instagrammy(needssomework)",
  "instgrammycarsnewsplit",
  "lessbluesmoreclues",
  "mooodyIguess",
  "parisbitch",
  "pinkyblue",
  "seemsalright",
  "seemsnice",
  "sooooooocrispy",
  "suprisinglyvintage",
  "suprisinglyvintageneutral",
  "suprisingvintage(blue",
  "unsre2",
  "unsure",
  "vintage nograin",
  "wild curves",
  "worksonpeople",
  "worksonpeople_",
  "yeeted-neggrain",
  "yeeted",
  "GRIFFIN'S CHROME TEXT"
];

export default function ApplyPresetScreen() {
  const navigation = useNavigation<ApplyPresetScreenNavigationProp>();
  const route = useRoute<ApplyPresetScreenRouteProp>();
  const { projectName, camera } = route.params;

  const [editingProjectName, setEditingProjectName] = useState(false);
  const [editingCamera, setEditingCamera] = useState(false);
  const [editingPreset, setEditingPreset] = useState(false);
  const [projectNameValue, setProjectNameValue] = useState(projectName);
  const [cameraValue, setCameraValue] = useState(camera);
  const [selectedPreset, setSelectedPreset] = useState(PRESETS[0]);
  const [presetStrength, setPresetStrength] = useState(50);
  const [grain, setGrain] = useState(false);
  const [lightLeaks, setLightLeaks] = useState(false);
  const [lensFlares, setLensFlares] = useState(false);
  const [showLivePreviewModal, setShowLivePreviewModal] = useState(false);

  const handleContinue = () => {
    console.log('ApplyPresetScreen navigating with:', {
      projectName: projectNameValue,
      camera: cameraValue,
      preset: selectedPreset,
    });
    navigation.navigate('ExportSettings', {
      projectName: projectNameValue,
      camera: cameraValue,
      preset: selectedPreset,
    });
  };

  const handleProjectNameEdit = () => {
    setEditingProjectName(true);
  };

  const handleProjectNameSave = () => {
    setEditingProjectName(false);
  };

  const handleCameraEdit = () => {
    setEditingCamera(true);
  };

  const handleCameraSave = () => {
    setEditingCamera(false);
  };

  const handlePresetEdit = () => {
    setEditingPreset(true);
  };

  const handlePresetSave = () => {
    setEditingPreset(false);
  };

  const handleOpenLivePreview = () => {
    setShowLivePreviewModal(true);
  };

  const handleCloseLivePreview = () => {
    setShowLivePreviewModal(false);
  };

  const handlePresetSelect = (preset: string) => {
    setSelectedPreset(preset);
  };

  const handlePresetStrengthChange = (value: number) => {
    setPresetStrength(value);
  };

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView style={styles.scrollView} showsVerticalScrollIndicator={false}>
        {/* Header with editable project info */}
        <HeaderWithBack
          title="Apply Preset"
          subtitle="Fine-tune your project settings"
        />

        {/* Editable Project Info */}
        <View style={styles.projectInfoSection}>
          {/* Project Name */}
          <View style={styles.infoRow}>
            <Text style={styles.infoLabel}>Project</Text>
            {editingProjectName ? (
              <View style={styles.editRow}>
                <TextInput
                  style={styles.editInput}
                  value={projectNameValue}
                  onChangeText={setProjectNameValue}
                  autoFocus
                  onBlur={handleProjectNameSave}
                  onSubmitEditing={handleProjectNameSave}
                />
                <TouchableOpacity onPress={handleProjectNameSave}>
                  <Ionicons name="checkmark" size={20} color="#175E4C" />
                </TouchableOpacity>
              </View>
            ) : (
              <TouchableOpacity style={styles.infoValue} onPress={handleProjectNameEdit}>
                <Text style={styles.infoValueText}>{projectNameValue}</Text>
                <Ionicons name="pencil" size={16} color="#666" />
              </TouchableOpacity>
            )}
          </View>

          {/* Camera */}
          <View style={styles.infoRow}>
            <Text style={styles.infoLabel}>Camera</Text>
            {editingCamera ? (
              <View style={styles.editRow}>
                <TextInput
                  style={styles.editInput}
                  value={cameraValue}
                  onChangeText={setCameraValue}
                  autoFocus
                  onBlur={handleCameraSave}
                  onSubmitEditing={handleCameraSave}
                />
                <TouchableOpacity onPress={handleCameraSave}>
                  <Ionicons name="checkmark" size={20} color="#175E4C" />
                </TouchableOpacity>
              </View>
            ) : (
              <TouchableOpacity style={styles.infoValue} onPress={handleCameraEdit}>
                <Text style={styles.infoValueText}>{cameraValue}</Text>
                <Ionicons name="pencil" size={16} color="#666" />
              </TouchableOpacity>
            )}
          </View>

          {/* Preset */}
          <View style={styles.infoRow}>
            <Text style={styles.infoLabel}>Preset</Text>
            {editingPreset ? (
              <View style={styles.editRow}>
                <TextInput
                  style={styles.editInput}
                  value={selectedPreset}
                  onChangeText={setSelectedPreset}
                  autoFocus
                  onBlur={handlePresetSave}
                  onSubmitEditing={handlePresetSave}
                />
                <TouchableOpacity onPress={handlePresetSave}>
                  <Ionicons name="checkmark" size={20} color="#175E4C" />
                </TouchableOpacity>
              </View>
            ) : (
              <TouchableOpacity style={styles.infoValue} onPress={handlePresetEdit}>
                <Text style={styles.infoValueText}>{selectedPreset}</Text>
                <Ionicons name="pencil" size={16} color="#666" />
              </TouchableOpacity>
            )}
          </View>
        </View>

        {/* Full-width image background with overlay */}
        <View style={styles.imageSection}>
          <View style={styles.imageBackground}>
            <View style={styles.imagePlaceholder}>
              <Ionicons name="image-outline" size={64} color="#CCC" />
              <Text style={styles.imagePlaceholderText}>Live Preview Image</Text>
              <Text style={styles.imageNote}>Using: 05-07-25-BGP-JH-04006.jpg</Text>
            </View>
          </View>
          
          {/* Bottom overlay for preset selection */}
          <View style={styles.presetOverlay}>
            <PresetCarousel
              presets={PRESETS}
              selectedPreset={selectedPreset}
              onPresetSelect={handlePresetSelect}
            />
          </View>
        </View>

        {/* Search/Add Presets Button */}
        <TouchableOpacity style={styles.searchButton} onPress={handleOpenLivePreview}>
          <Ionicons name="search" size={20} color="#175E4C" />
          <Text style={styles.searchButtonText}>Search & Add Presets</Text>
          <Ionicons name="add-circle" size={20} color="#175E4C" />
        </TouchableOpacity>

        {/* Preset Strength Slider */}
        <View style={styles.sliderSection}>
          <View style={styles.sliderHeader}>
            <Text style={styles.sliderLabel}>Preset Strength</Text>
            <Text style={styles.sliderValue}>{presetStrength}%</Text>
          </View>
          <Slider
            style={styles.slider}
            minimumValue={0}
            maximumValue={100}
            value={presetStrength}
            onValueChange={handlePresetStrengthChange}
            minimumTrackTintColor="#175E4C"
            maximumTrackTintColor="#E5E5E5"
          />
        </View>

        {/* Add-on Effects */}
        <View style={styles.effectsSection}>
          <Text style={styles.effectsTitle}>Add-on Effects</Text>
          
          <View style={styles.effectRow}>
            <Text style={styles.effectLabel}>Grain</Text>
            <Switch
              value={grain}
              onValueChange={setGrain}
              trackColor={{ false: '#E5E5E5', true: '#175E4C' }}
              thumbColor="#FFF"
            />
          </View>

          <View style={styles.effectRow}>
            <Text style={styles.effectLabel}>Light Leaks</Text>
            <Switch
              value={lightLeaks}
              onValueChange={setLightLeaks}
              trackColor={{ false: '#E5E5E5', true: '#175E4C' }}
              thumbColor="#FFF"
            />
          </View>

          <View style={styles.effectRow}>
            <Text style={styles.effectLabel}>Lens Flares</Text>
            <Switch
              value={lensFlares}
              onValueChange={setLensFlares}
              trackColor={{ false: '#E5E5E5', true: '#175E4C' }}
              thumbColor="#FFF"
            />
          </View>
        </View>

        {/* Pro Feature Divider */}
        <View style={styles.proDivider}>
          <View style={styles.proDividerLine} />
          <Text style={styles.proDividerText}>Further preset customization is available in PQTR Pro</Text>
          <View style={styles.proDividerLine} />
        </View>
      </ScrollView>

      {/* Footer */}
      <StepFooter
        buttonText="Continue"
        onPress={handleContinue}
        disabled={!selectedPreset}
      />

      {/* Live Preview Modal */}
      <PresetLivePreviewModal
        visible={showLivePreviewModal}
        onClose={handleCloseLivePreview}
        selectedPreset={selectedPreset}
        onPresetSelect={handlePresetSelect}
        presetStrength={presetStrength}
        onPresetStrengthChange={handlePresetStrengthChange}
      />
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#F9F6F1',
  },
  scrollView: {
    flex: 1,
  },
  projectInfoSection: {
    backgroundColor: '#FFF',
    margin: 20,
    borderRadius: 16,
    padding: 20,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.05,
    shadowRadius: 8,
    elevation: 2,
  },
  infoRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 12,
    borderBottomWidth: 1,
    borderBottomColor: '#F0F0F0',
  },
  infoLabel: {
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
  },
  infoValue: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  infoValueText: {
    fontSize: 16,
    color: '#175E4C',
    fontWeight: '500',
  },
  editRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 12,
  },
  editInput: {
    fontSize: 16,
    color: '#175E4C',
    fontWeight: '500',
    borderWidth: 1,
    borderColor: '#175E4C',
    borderRadius: 8,
    paddingHorizontal: 12,
    paddingVertical: 8,
    minWidth: 120,
  },
  imageSection: {
    margin: 20,
    borderRadius: 16,
    overflow: 'hidden',
    position: 'relative',
  },
  imageBackground: {
    width: '100%',
    height: 400,
    backgroundColor: '#F5F5F5',
  },
  imagePlaceholder: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  imagePlaceholderText: {
    marginTop: 16,
    fontSize: 16,
    color: '#999',
  },
  imageNote: {
    marginTop: 8,
    fontSize: 12,
    color: '#BBB',
    fontStyle: 'italic',
  },
  presetOverlay: {
    position: 'absolute',
    bottom: 0,
    left: 0,
    right: 0,
    backgroundColor: 'rgba(249, 246, 241, 0.95)',
    paddingVertical: 20,
    paddingHorizontal: 20,
  },
  searchButton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    backgroundColor: '#FFF',
    marginHorizontal: 20,
    marginBottom: 20,
    padding: 16,
    borderRadius: 12,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.05,
    shadowRadius: 8,
    elevation: 2,
  },
  searchButtonText: {
    fontSize: 16,
    fontWeight: '600',
    color: '#175E4C',
  },
  sliderSection: {
    backgroundColor: '#FFF',
    marginHorizontal: 20,
    marginBottom: 20,
    padding: 20,
    borderRadius: 16,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.05,
    shadowRadius: 8,
    elevation: 2,
  },
  sliderHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 16,
  },
  sliderLabel: {
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
  },
  sliderValue: {
    fontSize: 16,
    fontWeight: '600',
    color: '#175E4C',
  },
  slider: {
    width: '100%',
    height: 40,
  },
  sliderThumb: {
    backgroundColor: '#175E4C',
    width: 24,
    height: 24,
    borderRadius: 12,
  },
  effectsSection: {
    backgroundColor: '#FFF',
    marginHorizontal: 20,
    marginBottom: 20,
    padding: 20,
    borderRadius: 16,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.05,
    shadowRadius: 8,
    elevation: 2,
  },
  effectsTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#333',
    marginBottom: 16,
  },
  effectRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 12,
    borderBottomWidth: 1,
    borderBottomColor: '#F0F0F0',
  },
  effectLabel: {
    fontSize: 16,
    color: '#333',
  },
  proDivider: {
    flexDirection: 'row',
    alignItems: 'center',
    marginHorizontal: 20,
    marginBottom: 100,
    gap: 16,
  },
  proDividerLine: {
    flex: 1,
    height: 1,
    backgroundColor: '#E5E5E5',
  },
  proDividerText: {
    fontSize: 14,
    color: '#999',
    fontStyle: 'italic',
    textAlign: 'center',
  },
});
