// ----------------------
// STEP 4: Export Settings
// ----------------------

import React, { useState } from 'react';
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
import ExportFormatButtons from '../../components/create-project/ExportFormatButtons';
import ToggleSwitchRow from '../../components/create-project/ToggleSwitchRow';

export default function ExportSettingsScreen() {
  const navigation = useNavigation();
  const route = useRoute();
  const [exportFormats, setExportFormats] = useState<string[]>(['9x16']);
  const [autoExport, setAutoExport] = useState(true);
  
  // @ts-ignore - Route type will be properly typed when integrated
  const { projectName, camera, preset } = route.params || {};
  


  const handleFormatSelect = (formatId: string) => {
    setExportFormats(prev => {
      if (prev.includes(formatId)) {
        // Remove format if already selected
        return prev.filter(id => id !== formatId);
      } else {
        // Add format if not selected
        return [...prev, formatId];
      }
    });
  };

  const handleNext = () => {
    if (exportFormats.length === 0) {
      // Don't allow proceeding without selecting at least one format
      return;
    }
    
    // @ts-ignore - Navigation type will be properly typed when integrated
    navigation.navigate('FinalShoot', { 
      projectName, 
      camera, 
      preset, 
      exportFormats, 
      autoExport 
    });
  };

  const getPresetDisplayName = (presetId: string) => {
    if (!presetId || presetId === 'none' || presetId.trim() === '') return 'None';
    
    // Handle the actual preset names from your LRPRESETS folder
    // Remove common prefixes and format nicely
    let displayName = presetId;
    
    // Remove common prefixes
    if (displayName.startsWith('AMF1 - ')) {
      displayName = displayName.replace('AMF1 - ', '');
    } else if (displayName.startsWith('AM - ')) {
      displayName = displayName.replace('AM - ', '');
    }
    
    // Handle special cases
    if (displayName === 'AMF1_CHIP_APPROVED') {
      displayName = 'Chip Approved';
    } else if (displayName === 'AMF1 - 1NEWNAILEDIT') {
      displayName = 'New Nail Edit';
    } else if (displayName === 'AMF1 - 1NEWER') {
      displayName = 'Newer';
    } else if (displayName === 'AMF1 - B_W') {
      displayName = 'Black & White';
    } else if (displayName === 'AMF1 - 1NEW1') {
      displayName = 'New 1';
    } else if (displayName === 'AMF1 - 2023') {
      displayName = '2023';
    } else if (displayName === 'AMF1 - Ayo this do be kinda fire tho') {
      displayName = 'Ayo Fire';
    } else if (displayName === 'AMF1 - NEUTRAL CLEAN SOFT') {
      displayName = 'Neutral Clean Soft';
    } else if (displayName === 'AMF1 - Neutral Faded') {
      displayName = 'Neutral Faded';
    } else if (displayName === 'Soft but grainy teehee 2') {
      displayName = 'Soft Grainy 2';
    } else if (displayName === 'Soft but grainy teehee') {
      displayName = 'Soft Grainy';
    } else if (displayName === 'AMF1 - GRIME') {
      displayName = 'Grime';
    } else if (displayName === 'AMLESSAGGRESSIVE') {
      displayName = 'Less Aggressive';
    } else if (displayName === 'AMCLEAN') {
      displayName = 'Clean';
    } else if (displayName === 'AMF1 - ARTSY AF') {
      displayName = 'Artsy AF';
    } else if (displayName === 'AMF1 - CLEAN AS FUCK') {
      displayName = 'Clean AF';
    } else if (displayName === 'looksneutraltomehaha') {
      displayName = 'Neutral';
    } else if (displayName === 'AMF1 - Garage Preset') {
      displayName = 'Garage';
    } else if (displayName === 'AMF1 - PRESET 3 NO CURVES') {
      displayName = 'Preset 3 No Curves';
    } else if (displayName === 'AMF1 - JPEG') {
      displayName = 'JPEG';
    } else if (displayName === 'AMF1 - Preset 2') {
      displayName = 'Preset 2';
    } else if (displayName === 'AMF1 - Preset 1') {
      displayName = 'Preset 1';
    } else if (displayName === 'FLADDOCKCLUB') {
      displayName = 'Paddock Club';
    } else if (displayName === 'FLAT AS FUCK') {
      displayName = 'Flat AF';
    } else if (displayName === 'Greenedout') {
      displayName = 'Green Out';
    } else if (displayName === 'Hope Its clean') {
      displayName = 'Hope Clean';
    } else if (displayName === 'InstagrammyCars') {
      displayName = 'Instagram Cars';
    } else if (displayName === 'Its green we gooood') {
      displayName = 'Green Good';
    } else if (displayName === 'NEWSPLITGOODSHIT') {
      displayName = 'New Split';
    } else if (displayName === 'Noblu') {
      displayName = 'No Blue';
    } else if (displayName === 'PADDOCK - NEUTRAL_') {
      displayName = 'Paddock Neutral';
    } else if (displayName === 'PADDOCK') {
      displayName = 'Paddock';
    } else if (displayName === 'PinkHighlighterYpo') {
      displayName = 'Pink Highlighter';
    } else if (displayName === 'SEEMS ALRIGHT WFADE') {
      displayName = 'Seems Alright W/Fade';
    } else if (displayName === 'SEEMS ALRIGHT W_COLOUR') {
      displayName = 'Seems Alright W/Color';
    } else if (displayName === 'SVJ SPECIAL') {
      displayName = 'SVJ Special';
    } else if (displayName === 'TEXTURED VINTAGE') {
      displayName = 'Textured Vintage';
    } else if (displayName === 'WE COOOOL') {
      displayName = 'We Cool';
    } else if (displayName === 'banegredtion') {
      displayName = 'Bane Gredtion';
    } else if (displayName === 'bangernicercolopurs') {
      displayName = 'Banger Nice Colors';
    } else if (displayName === 'basic bitch IG') {
      displayName = 'Basic IG';
    } else if (displayName === 'beachedas(lessfade)') {
      displayName = 'Beached As (Less Fade)';
    } else if (displayName === 'beachedas') {
      displayName = 'Beached As';
    } else if (displayName === 'cars-icywhite') {
      displayName = 'Cars Icy White';
    } else if (displayName === 'carsicy-lessfadetho') {
      displayName = 'Cars Icy (Less Fade)';
    } else if (displayName === 'carsicydehaaazed') {
      displayName = 'Cars Icy Dehazed';
    } else if (displayName === 'carsicywithsomemids') {
      displayName = 'Cars Icy W/Mids';
    } else if (displayName === 'carsnewsplit') {
      displayName = 'Cars New Split';
    } else if (displayName === 'carstagramnoblues') {
      displayName = 'Carstagram No Blues';
    } else if (displayName === 'cityvibes') {
      displayName = 'City Vibes';
    } else if (displayName === 'cliffvibe') {
      displayName = 'Cliff Vibe';
    } else if (displayName === 'coolio desat') {
      displayName = 'Coolio Desat';
    } else if (displayName === 'darkicy') {
      displayName = 'Dark Icy';
    }
    
    return displayName;
  };

  // Debug logging to see what's being passed
  console.log('ExportSettingsScreen route params:', route.params);
  console.log('Preset value:', preset);
  console.log('Preset type:', typeof preset);
  console.log('Preset length:', preset ? preset.length : 'undefined');
  console.log('Preset display name:', getPresetDisplayName(preset || 'none'));

  const getCameraDisplayName = (cameraId: string) => {
    switch (cameraId) {
      case 'sony-a7iv': return "Peter's Sony A7iv";
      case 'canon-r5': return 'Canon R5';
      case 'nikon-z6ii': return 'Nikon Z6 II';
      default: return 'Camera';
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
          subtitle="Configure export settings"
        />
        
        {/* Summary Section */}
        <View style={styles.summaryContainer}>
          <Text style={styles.summaryTitle}>Project Summary</Text>
          <View style={styles.summaryCard}>
            <View style={styles.summaryRow}>
              <View style={styles.summaryIcon}>
                <Ionicons name="folder" size={20} color="#175E4C" />
              </View>
              <Text style={styles.summaryLabel}>Project</Text>
              <Text style={styles.summaryValue}>{projectName}</Text>
            </View>
            
            <View style={styles.summaryRow}>
              <View style={styles.summaryIcon}>
                <Ionicons name="camera" size={20} color="#175E4C" />
              </View>
              <Text style={styles.summaryLabel}>Camera</Text>
              <Text style={styles.summaryValue}>{getCameraDisplayName(camera)}</Text>
            </View>
            
            <View style={styles.summaryRow}>
              <View style={styles.summaryIcon}>
                <Ionicons name="color-palette" size={20} color="#175E4C" />
              </View>
              <Text style={styles.summaryLabel}>Preset</Text>
              <Text style={styles.summaryValue}>
                {getPresetDisplayName(preset || 'none')}
                {preset && preset.trim() ? ` (${preset})` : ''}
              </Text>
            </View>
          </View>
        </View>

        {/* Export Format Selection */}
        <ExportFormatButtons
          selectedFormats={exportFormats}
          onSelectFormat={handleFormatSelect}
        />

        {/* Auto Export Toggle */}
        <View style={styles.toggleContainer}>
          <ToggleSwitchRow
            label="Export Automatically"
            description="Photos will be automatically exported to your camera roll after processing"
            value={autoExport}
            onValueChange={setAutoExport}
          />
        </View>

        {/* Info Section */}
        <View style={styles.infoSection}>
          <View style={styles.infoCard}>
            <View style={styles.infoIcon}>
              <Ionicons name="cloud-upload" size={24} color="#175E4C" />
            </View>
            <View style={styles.infoContent}>
              <Text style={styles.infoTitle}>Smart Export</Text>
              <Text style={styles.infoDescription}>
                Your photos will be automatically processed and exported in the selected format. 
                You can always access the original files in the PQTR app.
              </Text>
            </View>
          </View>
        </View>
      </ScrollView>

      {/* Footer */}
      <StepFooter
        buttonText="Continue"
        onPress={handleNext}
        disabled={exportFormats.length === 0}
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
  summaryContainer: {
    paddingHorizontal: 24,
    marginBottom: 32,
  },
  summaryTitle: {
    fontSize: 18,
    fontWeight: '600',
    color: '#222',
    marginBottom: 16,
  },
  summaryCard: {
    backgroundColor: '#FFF',
    borderRadius: 20,
    padding: 24,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.08,
    shadowRadius: 16,
    elevation: 4,
  },
  summaryRow: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 12,
    borderBottomWidth: 1,
    borderBottomColor: '#F8F8F8',
  },
  summaryIcon: {
    width: 32,
    height: 32,
    borderRadius: 16,
    backgroundColor: '#F0F8F5',
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 16,
  },
  summaryLabel: {
    fontSize: 16,
    fontWeight: '500',
    color: '#666',
    flex: 1,
  },
  summaryValue: {
    fontSize: 16,
    fontWeight: '600',
    color: '#222',
    textAlign: 'right',
  },
  toggleContainer: {
    paddingHorizontal: 24,
    marginBottom: 32,
  },
  infoSection: {
    paddingHorizontal: 24,
    marginTop: 20,
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
