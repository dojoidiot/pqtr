import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { Feather } from '@expo/vector-icons';

export default function CameraStatus({ connected = true, lastSynced = "0s ago" }) {
  return (
    <View style={styles.container}>
      <Feather name="refresh-cw" size={16} color="white" style={{ marginRight: 6 }} />
      <Text style={styles.statusText}>
        Camera Connected • Synced {lastSynced}
      </Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    backgroundColor: '#22C55E',
    paddingVertical: 6,
    paddingHorizontal: 12,
    borderRadius: 12,
    flexDirection: 'row',
    alignItems: 'center',
    alignSelf: 'flex-start',
    marginTop: 8,
    marginBottom: 6,
    marginLeft: 4,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 2,
    elevation: 2,
  },
  statusText: {
    color: 'white',
    fontWeight: '600',
    fontSize: 12,
  },
});
