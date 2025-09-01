import React, { useRef, useEffect } from 'react';
import { View, Text, Animated, StyleSheet } from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { theme } from '../constants/theme';

const ProjectEmptyState: React.FC = () => {
  const fadeAnim = useRef(new Animated.Value(0)).current;
  const scaleAnim = useRef(new Animated.Value(0.8)).current;

  useEffect(() => {
    Animated.parallel([
      Animated.timing(fadeAnim, {
        toValue: 1,
        duration: 800,
        useNativeDriver: true,
      }),
      Animated.spring(scaleAnim, {
        toValue: 1,
        tension: 50,
        friction: 7,
        useNativeDriver: true,
      }),
    ]).start();
  }, [fadeAnim, scaleAnim]);

  return (
    <Animated.View 
      style={[
        styles.container,
        {
          opacity: fadeAnim,
          transform: [{ scale: scaleAnim }],
        }
      ]}
      accessibilityRole="text"
      accessibilityLabel="No images yet. Tap the plus icon to add photos or import from your gallery."
    >
      <View style={styles.iconContainer}>
        <Ionicons name="camera" size={80} color={theme.colors.gray} />
      </View>
      
      <Text style={styles.title}>No images yet</Text>
      
      <Text style={styles.subtitle}>
        Tap the + icon to add photos or import from your gallery.
      </Text>
    </Animated.View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingHorizontal: theme.spacing.l,
  },
  iconContainer: {
    marginBottom: theme.spacing.l,
    opacity: 0.6,
  },
  title: {
    fontSize: 24,
    fontWeight: '700',
    color: theme.colors.accent,
    marginBottom: theme.spacing.m,
    textAlign: 'center',
    letterSpacing: -0.5,
  },
  subtitle: {
    fontSize: 16,
    fontWeight: '500',
    color: theme.colors.gray,
    textAlign: 'center',
    lineHeight: 22,
    letterSpacing: 0.2,
    maxWidth: 280,
  },
});

export default ProjectEmptyState;
