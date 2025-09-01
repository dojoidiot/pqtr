import React, { useState } from 'react';
import { View, Text, TouchableOpacity, StyleSheet } from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';

interface ToggleControlProps {
  label: string;
  onToggle?: (isActive: boolean) => void;
  defaultActive?: boolean;
}

export default function ToggleControl({ 
  label, 
  onToggle, 
  defaultActive = false 
}: ToggleControlProps) {
  const [isActive, setIsActive] = useState(defaultActive);

  const handleToggle = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    const newState = !isActive;
    setIsActive(newState);
    onToggle?.(newState);
  };

  return (
    <TouchableOpacity
      style={[
        styles.container,
        isActive && styles.containerActive
      ]}
      onPress={handleToggle}
      activeOpacity={0.8}
      accessibilityRole="button"
      accessibilityLabel={`${isActive ? 'Disable' : 'Enable'} ${label}`}
      accessibilityState={{ selected: isActive }}
    >
      <Ionicons 
        name={isActive ? "checkmark-circle" : "square-outline"} 
        size={20} 
        color={isActive ? "#3B82F6" : "#6B7280"} 
        style={styles.icon}
      />
      <Text style={[
        styles.label,
        isActive && styles.labelActive
      ]}>
        {isActive ? 'Selecting' : label}
      </Text>
    </TouchableOpacity>
  );
}

const styles = StyleSheet.create({
  container: {
    backgroundColor: '#F3F4F6',
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    paddingVertical: 12,
    paddingHorizontal: 20,
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#E5E7EB',
    minHeight: 48,
    minWidth: 100,
  },
  containerActive: {
    backgroundColor: '#EBF4FF',
    borderColor: '#3B82F6',
  },
  icon: {
    marginRight: 8,
  },
  label: {
    color: '#6B7280',
    fontSize: 14,
    fontWeight: '600',
  },
  labelActive: {
    color: '#3B82F6',
  },
});
