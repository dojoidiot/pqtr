import React from 'react';
import { TouchableOpacity, Text, StyleSheet } from 'react-native';

interface GradientButtonProps {
  title: string;
  onPress: () => void;
}

export default function GradientButton({ title, onPress }: GradientButtonProps) {
  return (
    <TouchableOpacity style={styles.button} onPress={onPress} activeOpacity={0.8}>
      <Text style={styles.text}>{title}</Text>
    </TouchableOpacity>
  );
}

const styles = StyleSheet.create({
  button: {
    backgroundColor: '#175E4C',
    paddingVertical: 18,
    borderRadius: 14,
    alignItems: 'center',
  },
  text: {
    color: '#fff',
    fontSize: 16,
    fontWeight: '600',
  },
});
