import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { Ionicons } from '@expo/vector-icons';

export default function ProjectInfoContent() {
  return (
    <View style={styles.container}>
      <View style={styles.infoRow}>
        <Ionicons name="calendar-outline" size={16} color="#6B7280" />
        <Text style={styles.infoLabel}>Date:</Text>
        <Text style={styles.infoValue}>December 15, 2024</Text>
      </View>
      
      <View style={styles.infoRow}>
        <Ionicons name="location-outline" size={16} color="#6B7280" />
        <Text style={styles.infoLabel}>Location:</Text>
        <Text style={styles.infoValue}>Yas Marina Circuit, Abu Dhabi</Text>
      </View>
      
      <View style={styles.infoRow}>
        <Ionicons name="images-outline" size={16} color="#6B7280" />
        <Text style={styles.infoLabel}>Photos:</Text>
        <Text style={styles.infoValue}>247 images</Text>
      </View>
      
      <View style={styles.infoRow}>
        <Ionicons name="people-outline" size={16} color="#6B7280" />
        <Text style={styles.infoLabel}>Collaborators:</Text>
        <Text style={styles.infoValue}>3 team members</Text>
      </View>
      
      <View style={styles.infoRow}>
        <Ionicons name="color-palette-outline" size={16} color="#6B7280" />
        <Text style={styles.infoLabel}>Preset:</Text>
        <Text style={styles.infoValue}>Vivid Landscape</Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    gap: 12,
  },
  infoRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  infoLabel: {
    fontSize: 14,
    fontWeight: '500',
    color: '#374151',
    minWidth: 80,
  },
  infoValue: {
    fontSize: 14,
    color: '#1F2937',
    flex: 1,
  },
});
