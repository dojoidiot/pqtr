// ----------------------
// STEP 5: Final Screen – Shoot
// ----------------------

import React from 'react';
import {
  View,
  Text,
  StyleSheet,
  SafeAreaView,
  ScrollView,
} from 'react-native';
import { useNavigation, useRoute } from '@react-navigation/native';
import { Ionicons } from '@expo/vector-icons';
import HeaderWithBack from '../../components/create-project/HeaderWithBack';
import StepFooter from '../../components/create-project/StepFooter';

export default function FinalShootScreen() {
  const navigation = useNavigation();
  const route = useRoute();
  
  // @ts-ignore - Route type will be properly typed when integrated
  const { projectName, camera, preset, exportFormats, autoExport } = route.params || {};

  const handleShoot = () => {
    // Here you would typically:
    // 1. Save the project configuration
    // 2. Navigate to camera/photo capture
    // 3. Or return to main app
    navigation.navigate('Home'); // Navigate back to home screen
  };

  const getPresetDisplayName = (presetId: string) => {
    switch (presetId) {
      case 'none': return 'None';
      case 'bw': return 'Black & White';
      case 'grain': return 'Grain + Flares';
      case 'cinematic': return 'Cinematic';
      case 'vintage': return 'Vintage';
      case 'clean': return 'Clean';
      default: return 'None';
    }
  };

  const getCameraDisplayName = (cameraId: string) => {
    switch (cameraId) {
      case 'sony-a7iv': return "Peter's Sony A7iv";
      case 'canon-r5': return 'Canon R5';
      case 'nikon-z6ii': return 'Nikon Z6 II';
      default: return 'Camera';
    }
  };

  const getFormatDisplayName = (formatId: string) => {
    switch (formatId) {
      case '9x16': return '9:16 Portrait';
      case '16x9': return '16:9 Landscape';
      case '1x1': return '1:1 Square';
      default: return '9:16 Portrait';
    }
  };

  const getFormatsDisplayText = (formats: string[]) => {
    if (!formats || formats.length === 0) return '9:16 Portrait';
    if (formats.length === 1) return getFormatDisplayName(formats[0]);
    return `${formats.length} formats selected`;
  };

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView 
        style={styles.scrollView}
        contentContainerStyle={styles.scrollContent}
        showsVerticalScrollIndicator={false}
      >
        {/* Header */}
        <HeaderWithBack 
          title={projectName || 'Project'} 
          subtitle="Ready to shoot"
        />
        
        {/* Success Message */}
        <View style={styles.successContainer}>
          <View style={styles.successIcon}>
            <Ionicons name="checkmark-circle" size={48} color="#175E4C" />
          </View>
          <Text style={styles.successTitle}>Project Created!</Text>
          <Text style={styles.successSubtitle}>
            Your project is ready and configured. Time to capture some amazing photos.
          </Text>
        </View>

        {/* Project Recap */}
        <View style={styles.recapContainer}>
          <Text style={styles.recapTitle}>Project Configuration</Text>
          <View style={styles.recapCard}>
            <View style={styles.recapRow}>
              <View style={styles.recapIcon}>
                <Ionicons name="folder" size={20} color="#175E4C" />
              </View>
              <Text style={styles.recapLabel}>Project Name</Text>
              <Text style={styles.recapValue}>{projectName}</Text>
            </View>
            
            <View style={styles.recapRow}>
              <View style={styles.recapIcon}>
                <Ionicons name="camera" size={20} color="#175E4C" />
              </View>
              <Text style={styles.recapLabel}>Camera</Text>
              <Text style={styles.recapValue}>{getCameraDisplayName(camera)}</Text>
            </View>
            
            <View style={styles.recapRow}>
              <View style={styles.recapIcon}>
                <Ionicons name="color-palette" size={20} color="#175E4C" />
              </View>
              <Text style={styles.recapLabel}>Preset</Text>
              <Text style={styles.recapValue}>{getPresetDisplayName(preset)}</Text>
            </View>
            
            <View style={styles.recapRow}>
              <View style={styles.recapIcon}>
                <Ionicons name="resize" size={20} color="#175E4C" />
              </View>
              <Text style={styles.recapLabel}>Export Format</Text>
              <Text style={styles.recapValue}>{getFormatsDisplayText(exportFormats)}</Text>
            </View>
            
            <View style={styles.recapRow}>
              <View style={styles.recapIcon}>
                <Ionicons name="cloud-upload" size={20} color="#175E4C" />
              </View>
              <Text style={styles.recapLabel}>Auto Export</Text>
              <Text style={styles.recapValue}>
                {autoExport ? 'Enabled' : 'Disabled'}
              </Text>
            </View>
          </View>
        </View>

        {/* Next Steps */}
        <View style={styles.nextStepsContainer}>
          <Text style={styles.nextStepsTitle}>What happens next?</Text>
          <View style={styles.nextStepsCard}>
            <View style={styles.stepItem}>
              <View style={styles.stepNumber}>1</View>
              <View style={styles.stepContent}>
                <Text style={styles.stepTitle}>Connect Camera</Text>
                <Text style={styles.stepDescription}>
                  Ensure your camera is connected and synced with the app
                </Text>
              </View>
            </View>
            
            <View style={styles.stepItem}>
              <View style={styles.stepNumber}>2</View>
              <View style={styles.stepContent}>
                <Text style={styles.stepTitle}>Start Shooting</Text>
                <Text style={styles.stepDescription}>
                  Photos will automatically apply your selected preset and export settings
                </Text>
              </View>
            </View>
            
            <View style={styles.stepItem}>
              <View style={styles.stepNumber}>3</View>
              <View style={styles.stepContent}>
                <Text style={styles.stepTitle}>Review & Edit</Text>
                <Text style={styles.stepDescription}>
                  Access all photos in your project for further editing and organization
                </Text>
              </View>
            </View>
          </View>
        </View>
      </ScrollView>

      {/* Footer with Shoot Button */}
      <StepFooter
        buttonText="Shoot!"
        onPress={handleShoot}
        isFinal={true}
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
  scrollContent: {
    flexGrow: 1,
    paddingBottom: 120, // Space for footer
  },
  successContainer: {
    alignItems: 'center',
    paddingHorizontal: 24,
    marginBottom: 40,
  },
  successIcon: {
    marginBottom: 20,
  },
  successTitle: {
    fontSize: 28,
    fontWeight: '700',
    color: '#222',
    marginBottom: 12,
    textAlign: 'center',
  },
  successSubtitle: {
    fontSize: 16,
    color: '#666',
    textAlign: 'center',
    lineHeight: 24,
    paddingHorizontal: 20,
  },
  recapContainer: {
    paddingHorizontal: 24,
    marginBottom: 32,
  },
  recapTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#222',
    marginBottom: 16,
  },
  recapCard: {
    backgroundColor: '#FFF',
    borderRadius: 20,
    padding: 24,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.08,
    shadowRadius: 16,
    elevation: 4,
  },
  recapRow: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 16,
    borderBottomWidth: 1,
    borderBottomColor: '#F8F8F8',
  },
  recapIcon: {
    width: 32,
    height: 32,
    borderRadius: 16,
    backgroundColor: '#F0F8F5',
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 16,
  },
  recapLabel: {
    fontSize: 16,
    fontWeight: '500',
    color: '#666',
    flex: 1,
  },
  recapValue: {
    fontSize: 16,
    fontWeight: '600',
    color: '#222',
    textAlign: 'right',
  },
  nextStepsContainer: {
    paddingHorizontal: 24,
    marginTop: 20,
  },
  nextStepsTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#222',
    marginBottom: 16,
  },
  nextStepsCard: {
    backgroundColor: '#FFF',
    borderRadius: 20,
    padding: 24,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.08,
    shadowRadius: 16,
    elevation: 4,
  },
  stepItem: {
    flexDirection: 'row',
    alignItems: 'flex-start',
    marginBottom: 24,
  },
  stepNumber: {
    width: 32,
    height: 32,
    borderRadius: 16,
    backgroundColor: '#175E4C',
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 16,
    marginTop: 2,
  },
  stepContent: {
    flex: 1,
  },
  stepTitle: {
    fontSize: 16,
    fontWeight: '600',
    color: '#222',
    marginBottom: 4,
  },
  stepDescription: {
    fontSize: 14,
    color: '#666',
    lineHeight: 20,
  },
});
