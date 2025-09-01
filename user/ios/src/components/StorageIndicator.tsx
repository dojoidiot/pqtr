import React, { useEffect, useRef } from 'react';
import { Text, View, Animated, StyleSheet } from 'react-native';
import { theme } from '../constants/theme';

interface StorageIndicatorProps {
  usage: number; // 0-100
}

const StorageIndicator: React.FC<StorageIndicatorProps> = ({ usage }) => {
  const progressAnim = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    Animated.timing(progressAnim, {
      toValue: usage,
      duration: 500,
      useNativeDriver: false,
    }).start();
  }, [usage, progressAnim]);

  const progressWidth = progressAnim.interpolate({
    inputRange: [0, 100],
    outputRange: ['0%', '100%'],
  });

  return (
    <View style={styles.container}>
      <View style={styles.headerRow}>
        <Text style={styles.storageLabel}>
          Storage Used: {usage}%
        </Text>
        <Text style={styles.storageValue}>
          {usage}%
        </Text>
      </View>
      <View style={styles.progressBarContainer}>
        <Animated.View
          style={[styles.progressBar, { width: progressWidth }]}
          accessibilityLabel={`Progress bar showing ${usage} percent storage used`}
          accessibilityRole="progressbar"
          accessibilityValue={{
            min: 0,
            max: 100,
            now: usage,
          }}
        />
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    marginBottom: theme.spacing.m,
  },
  headerRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: theme.spacing.s,
  },
  storageLabel: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.secondary,
    fontWeight: '500',
  },
  storageValue: {
    fontSize: theme.fontSizes.caption,
    color: theme.colors.text.primary,
    fontWeight: '600',
  },
  progressBarContainer: {
    height: 4,
    backgroundColor: theme.colors.lightGray,
    borderRadius: 2,
    overflow: 'hidden',
  },
  progressBar: {
    height: '100%',
    backgroundColor: theme.colors.green,
    borderRadius: 2,
  },
});

export default StorageIndicator; 