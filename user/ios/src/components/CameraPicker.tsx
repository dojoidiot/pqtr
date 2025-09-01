import React from 'react';
import { View, Text, TouchableOpacity, StyleSheet } from 'react-native';

interface CameraPickerProps {
  selected: string | null;
  onSelect: (camera: string) => void;
}

export default function CameraPicker({ selected, onSelect }: CameraPickerProps) {
  return (
    <View style={styles.container}>
      <Text style={styles.label}>Select Camera</Text>
      <TouchableOpacity style={styles.button} onPress={() => onSelect('Sony A7IV')}>
        <Text style={styles.text}>{selected || 'Tap to select camera'}</Text>
      </TouchableOpacity>
    </View>
  );
}

const styles = StyleSheet.create({
  container: { 
    marginBottom: 24 
  },
  label: { 
    marginBottom: 6, 
    fontSize: 14, 
    color: '#444' 
  },
  button: {
    backgroundColor: '#FFF',
    padding: 14,
    borderRadius: 14,
  },
  text: { 
    fontSize: 16, 
    color: '#111' 
  },
});
