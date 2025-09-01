import React, { useState } from 'react';
import { View, Text, TouchableOpacity, StyleSheet, Modal } from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import Animated, { 
  useSharedValue, 
  useAnimatedStyle, 
  withSpring 
} from 'react-native-reanimated';

interface Camera {
  id: string;
  name: string;
  model: string;
}

interface CameraSelectorDropdownProps {
  selectedCamera: string | null;
  onSelectCamera: (cameraId: string) => void;
}

const mockCameras: Camera[] = [
  { id: 'sony-a7iv', name: "Peter's Sony A7iv", model: 'Sony A7 IV' },
  { id: 'canon-r5', name: 'Canon R5', model: 'Canon EOS R5' },
  { id: 'nikon-z6ii', name: 'Nikon Z6 II', model: 'Nikon Z6 II' },
];

export default function CameraSelectorDropdown({ selectedCamera, onSelectCamera }: CameraSelectorDropdownProps) {
  const [isModalVisible, setIsModalVisible] = useState(false);
  const [isExpanded, setIsExpanded] = useState(false);
  const rotation = useSharedValue(0);

  const selectedCameraData = mockCameras.find(cam => cam.id === selectedCamera);

  const toggleExpanded = () => {
    setIsExpanded(!isExpanded);
    rotation.value = withSpring(isExpanded ? 0 : 180, {
      damping: 15,
      stiffness: 300,
    });
  };

  const animatedStyle = useAnimatedStyle(() => ({
    transform: [{ rotate: `${rotation.value}deg` }],
  }));

  const handleCameraSelect = (cameraId: string) => {
    onSelectCamera(cameraId);
    setIsModalVisible(false);
  };

  return (
    <View style={styles.container}>
      <Text style={styles.label}>Select Camera</Text>
      
      <TouchableOpacity 
        style={styles.selector} 
        onPress={toggleExpanded}
        activeOpacity={0.8}
      >
        <View style={styles.selectorContent}>
          {selectedCameraData ? (
            <View>
              <Text style={styles.selectedCameraName}>{selectedCameraData.name}</Text>
              <Text style={styles.selectedCameraModel}>{selectedCameraData.model}</Text>
            </View>
          ) : (
            <Text style={styles.placeholder}>Tap to select camera</Text>
          )}
        </View>
        
        <Animated.View style={[styles.chevron, animatedStyle]}>
          <Ionicons name="chevron-down" size={20} color="#666" />
        </Animated.View>
      </TouchableOpacity>

      {isExpanded && (
        <View style={styles.dropdown}>
          {mockCameras.map((camera) => (
            <TouchableOpacity
              key={camera.id}
              style={[
                styles.cameraOption,
                selectedCamera === camera.id && styles.cameraOptionSelected
              ]}
              onPress={() => handleCameraSelect(camera.id)}
              activeOpacity={0.7}
            >
              <View>
                <Text style={[
                  styles.cameraOptionName,
                  selectedCamera === camera.id && styles.cameraOptionNameSelected
                ]}>
                  {camera.name}
                </Text>
                <Text style={[
                  styles.cameraOptionModel,
                  selectedCamera === camera.id && styles.cameraOptionModelSelected
                ]}>
                  {camera.model}
                </Text>
              </View>
              
              {selectedCamera === camera.id && (
                <Ionicons name="checkmark" size={20} color="#175E4C" />
              )}
            </TouchableOpacity>
          ))}
          
          <TouchableOpacity 
            style={styles.addCameraButton}
            onPress={() => setIsModalVisible(true)}
            activeOpacity={0.7}
          >
            <Ionicons name="add-circle-outline" size={20} color="#175E4C" />
            <Text style={styles.addCameraText}>Add New Camera</Text>
          </TouchableOpacity>
        </View>
      )}

      <Modal
        visible={isModalVisible}
        transparent
        animationType="fade"
        onRequestClose={() => setIsModalVisible(false)}
      >
        <View style={styles.modalOverlay}>
          <View style={styles.modalContent}>
            <Text style={styles.modalTitle}>Add New Camera</Text>
            <Text style={styles.modalSubtitle}>Camera setup will be implemented in the next iteration</Text>
            <TouchableOpacity 
              style={styles.modalButton}
              onPress={() => setIsModalVisible(false)}
            >
              <Text style={styles.modalButtonText}>Got it</Text>
            </TouchableOpacity>
          </View>
        </View>
      </Modal>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    marginBottom: 32,
  },
  label: {
    fontSize: 16,
    fontWeight: '500',
    color: '#444',
    marginBottom: 12,
  },
  selector: {
    backgroundColor: '#FFF',
    borderRadius: 16,
    paddingHorizontal: 20,
    paddingVertical: 16,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    borderWidth: 1,
    borderColor: '#F0F0F0',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.05,
    shadowRadius: 8,
    elevation: 2,
  },
  selectorContent: {
    flex: 1,
  },
  selectedCameraName: {
    fontSize: 16,
    fontWeight: '600',
    color: '#222',
    marginBottom: 2,
  },
  selectedCameraModel: {
    fontSize: 14,
    color: '#666',
  },
  placeholder: {
    fontSize: 16,
    color: '#999',
  },
  chevron: {
    marginLeft: 12,
  },
  dropdown: {
    backgroundColor: '#FFF',
    borderRadius: 16,
    marginTop: 8,
    paddingVertical: 8,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.1,
    shadowRadius: 12,
    elevation: 8,
  },
  cameraOption: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingHorizontal: 20,
    paddingVertical: 16,
    borderBottomWidth: 1,
    borderBottomColor: '#F8F8F8',
  },
  cameraOptionSelected: {
    backgroundColor: '#F8F8F8',
  },
  cameraOptionName: {
    fontSize: 16,
    fontWeight: '500',
    color: '#222',
    marginBottom: 2,
  },
  cameraOptionNameSelected: {
    color: '#175E4C',
    fontWeight: '600',
  },
  cameraOptionModel: {
    fontSize: 14,
    color: '#666',
  },
  cameraOptionModelSelected: {
    color: '#175E4C',
  },
  addCameraButton: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 20,
    paddingVertical: 16,
  },
  addCameraText: {
    fontSize: 16,
    fontWeight: '500',
    color: '#175E4C',
    marginLeft: 12,
  },
  modalOverlay: {
    flex: 1,
    backgroundColor: 'rgba(0, 0, 0, 0.5)',
    justifyContent: 'center',
    alignItems: 'center',
  },
  modalContent: {
    backgroundColor: '#FFF',
    borderRadius: 20,
    padding: 24,
    marginHorizontal: 32,
    alignItems: 'center',
  },
  modalTitle: {
    fontSize: 20,
    fontWeight: '600',
    color: '#222',
    marginBottom: 8,
  },
  modalSubtitle: {
    fontSize: 16,
    color: '#666',
    textAlign: 'center',
    marginBottom: 24,
    lineHeight: 22,
  },
  modalButton: {
    backgroundColor: '#175E4C',
    paddingHorizontal: 24,
    paddingVertical: 12,
    borderRadius: 12,
  },
  modalButtonText: {
    color: '#FFF',
    fontSize: 16,
    fontWeight: '600',
  },
});
