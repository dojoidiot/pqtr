import React from 'react';
import { View, Text, TouchableOpacity, StyleSheet } from 'react-native';

const formats = ['9x16', '16x9', '1x1'];

interface ExportOptionsProps {
  selected: string;
  onSelect: (format: string) => void;
}

export default function ExportOptions({ selected, onSelect }: ExportOptionsProps) {
  return (
    <View style={styles.container}>
      <Text style={styles.label}>Export Format</Text>
      <View style={styles.row}>
        {formats.map((format) => (
          <TouchableOpacity
            key={format}
            style={[
              styles.option,
              selected === format && styles.active,
            ]}
            onPress={() => onSelect(format)}
          >
            <Text style={[
              styles.optionText,
              selected === format && styles.activeOptionText
            ]}>
              {format}
            </Text>
          </TouchableOpacity>
        ))}
      </View>
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
  row: { 
    flexDirection: 'row', 
    justifyContent: 'space-between',
    gap: 8,
  },
  option: {
    flex: 1,
    paddingVertical: 12,
    borderRadius: 12,
    backgroundColor: '#EEE',
    alignItems: 'center',
  },
  active: {
    backgroundColor: '#175E4C',
  },
  optionText: {
    color: '#222',
    fontWeight: '500',
    fontSize: 14,
  },
  activeOptionText: {
    color: '#fff',
  },
});
