// File: components/UploadProgressPill.tsx
// Expo Go friendly. No native modules. Animated linear fill + status icons.

import React, { useEffect, useRef } from 'react';
import { View, Text, StyleSheet, Animated } from 'react-native';
import { Ionicons } from '@expo/vector-icons';

type Status = 'queued' | 'uploading' | 'paused' | 'error' | 'synced';

export type UploadProgressPillProps = {
  progress?: number;         // 0..1
  status?: Status;
  compact?: boolean;         // smaller size for tight corners
};

export default function UploadProgressPill({
  progress = 0,
  status,
  compact = false,
}: UploadProgressPillProps) {
  const p = Math.max(0, Math.min(1, progress));
  const isSynced = status === 'synced' || p >= 1;
  const pillH = compact ? 26 : 30;
  const pillPadH = compact ? 8 : 10;
  const iconSize = compact ? 14 : 16;

  // animate width fill
  const fill = useRef(new Animated.Value(p)).current;
  useEffect(() => {
    Animated.timing(fill, { toValue: p, duration: 280, useNativeDriver: false }).start();
  }, [p, fill]);

  // derive colors by status
  const palette = (() => {
    if (isSynced) return { bg: 'rgba(255,255,255,0.92)', fill: '#2D9C5A', text: '#0f1a12', icon: '#2D9C5A' };
    switch (status) {
      case 'error': return { bg: 'rgba(255,255,255,0.92)', fill: '#DC2626', text: '#1f1f1f', icon: '#DC2626' };
      case 'paused': return { bg: 'rgba(255,255,255,0.92)', fill: '#9CA3AF', text: '#1f1f1f', icon: '#6B7280' };
      case 'queued': return { bg: 'rgba(255,255,255,0.92)', fill: '#A7F3D0', text: '#1f1f1f', icon: '#10B981' };
      default:       return { bg: 'rgba(255,255,255,0.92)', fill: '#1E8E3E', text: '#1f1f1f', icon: '#1E8E3E' }; // uploading
    }
  })();

  // compute fill width via interpolation (0..1 → 0..100%)
  const fillWidth = fill.interpolate({ inputRange: [0, 1], outputRange: ['0%', '100%'] });

  // choose icon by status
  const iconName = isSynced
    ? 'checkmark'
    : status === 'paused'
    ? 'pause'
    : status === 'error'
    ? 'alert-circle'
    : status === 'queued'
    ? 'cloud-upload'
    : 'cloud-upload'; // uploading

  const label = isSynced ? 'Synced' : `${Math.round(p * 100)}%`;

  return (
    <View style={[styles.wrap, { height: pillH, backgroundColor: palette.bg, paddingHorizontal: pillPadH }]}>
      {/* animated fill layer */}
      <Animated.View style={[styles.fill, { backgroundColor: palette.fill, width: fillWidth }]} />

      {/* content row on top */}
      <View style={styles.content}>
        <Ionicons name={iconName as any} size={iconSize} color={palette.icon} />
        <Text style={[styles.text, { color: palette.text }]}>{label}</Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: {
    borderRadius: 999,
    overflow: 'hidden',
    alignSelf: 'flex-start',
  },
  fill: {
    ...StyleSheet.absoluteFillObject,
    borderRadius: 999,
  },
  content: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    zIndex: 2,
  },
  text: {
    fontSize: 13,
    fontWeight: '700',
  },
});
