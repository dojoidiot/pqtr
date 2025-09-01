// ----------------------
// STEP 2: Select Camera
// ----------------------

import React, { useState } from 'react';
import {
  View,
  StyleSheet,
  SafeAreaView,
  ScrollView,
  Text,
} from 'react-native';
import { useNavigation, useRoute } from '@react-navigation/native';
import HeaderWithBack from '../../components/create-project/HeaderWithBack';
import StepFooter from '../../components/create-project/StepFooter';
import CameraSelectorDropdown from '../../components/create-project/CameraSelectorDropdown';

export default function SelectCameraScreen() {
  const navigation = useNavigation();
  const route = useRoute();
  const [selectedCamera, setSelectedCamera] = useState<string | null>(null);
  
  // @ts-ignore - Route type will be properly typed when integrated
  const { projectName } = route.params || {};

  const handleNext = () => {
    if (selectedCamera) {
      // @ts-ignore - Navigation type will be properly typed when integrated
      navigation.navigate('ApplyPreset', { 
        projectName, 
        camera: selectedCamera 
      });
    }
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
          subtitle="Select camera to sync with this project"
        />
        
        {/* Camera Selection */}
        <View style={styles.content}>
          <CameraSelectorDropdown
            selectedCamera={selectedCamera}
            onSelectCamera={setSelectedCamera}
          />
          
          {/* Info Section */}
          <View style={styles.infoSection}>
            <View style={styles.infoCard}>
              <View style={styles.infoIcon}>
                <View style={styles.iconCircle}>
                  <View style={styles.iconDot} />
                </View>
              </View>
              <View style={styles.infoContent}>
                <Text style={styles.infoTitle}>Camera Sync</Text>
                <Text style={styles.infoDescription}>
                  Your selected camera will automatically sync photos to this project. 
                  You can change this later in project settings.
                </Text>
              </View>
            </View>
          </View>
        </View>
      </ScrollView>

      {/* Footer */}
      <StepFooter
        buttonText="Continue"
        onPress={handleNext}
        disabled={!selectedCamera}
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
  content: {
    paddingHorizontal: 24,
    marginTop: 20,
  },
  infoSection: {
    marginTop: 40,
  },
  infoCard: {
    backgroundColor: '#FFF',
    borderRadius: 20,
    padding: 24,
    flexDirection: 'row',
    alignItems: 'flex-start',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.08,
    shadowRadius: 16,
    elevation: 4,
  },
  infoIcon: {
    marginRight: 16,
    marginTop: 4,
  },
  iconCircle: {
    width: 32,
    height: 32,
    borderRadius: 16,
    backgroundColor: '#F0F8F5',
    justifyContent: 'center',
    alignItems: 'center',
  },
  iconDot: {
    width: 8,
    height: 8,
    borderRadius: 4,
    backgroundColor: '#175E4C',
  },
  infoContent: {
    flex: 1,
  },
  infoTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#222',
    marginBottom: 8,
  },
  infoDescription: {
    fontSize: 15,
    color: '#666',
    lineHeight: 22,
  },
});
