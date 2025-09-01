import React, { useState, useEffect, useRef } from 'react';
import { View, Text, TouchableOpacity, Animated, StyleSheet } from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

interface CameraStatusBadgeProps {
  connected: boolean;
  lastSynced: string;
}

const CameraStatusBadge: React.FC<CameraStatusBadgeProps> = ({ connected, lastSynced }) => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [showTooltip, setShowTooltip] = useState(false);
  const rotateAnim = useRef(new Animated.Value(0)).current;
  const fadeAnim = useRef(new Animated.Value(0)).current;
  const pulseAnim = useRef(new Animated.Value(1)).current;

  // Update time every 10 seconds
  useEffect(() => {
    const interval = setInterval(() => {
      setCurrentTime(new Date());
    }, 10000);

    return () => clearInterval(interval);
  }, []);

  // Animate sync icon rotation and pulse
  useEffect(() => {
    if (connected) {
      // Rotation animation
      Animated.timing(rotateAnim, {
        toValue: 1,
        duration: 1000,
        useNativeDriver: true,
      }).start(() => {
        rotateAnim.setValue(0);
      });

      // Pulse animation for live syncing
      Animated.loop(
        Animated.sequence([
          Animated.timing(pulseAnim, {
            toValue: 1.1,
            duration: 1000,
            useNativeDriver: true,
          }),
          Animated.timing(pulseAnim, {
            toValue: 1,
            duration: 1000,
            useNativeDriver: true,
          }),
        ])
      ).start();
    }
  }, [lastSynced, connected, rotateAnim, pulseAnim]);

  // Animate tooltip fade
  useEffect(() => {
    if (showTooltip) {
      Animated.timing(fadeAnim, {
        toValue: 1,
        duration: 200,
        useNativeDriver: true,
      }).start();
    } else {
      Animated.timing(fadeAnim, {
        toValue: 0,
        duration: 200,
        useNativeDriver: true,
      }).start();
    }
  }, [showTooltip, fadeAnim]);

  const handlePress = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    setShowTooltip(!showTooltip);
  };

  const getTimeAgo = (timestamp: string) => {
    const now = currentTime;
    const date = new Date(timestamp);
    const diffInSeconds = Math.floor((now.getTime() - date.getTime()) / 1000);

    if (diffInSeconds < 60) {
      return `${diffInSeconds}s ago`;
    } else if (diffInSeconds < 3600) {
      return `${Math.floor(diffInSeconds / 60)}m ago`;
    } else if (diffInSeconds < 86400) {
      return `${Math.floor(diffInSeconds / 3600)}h ago`;
    } else {
      return `${Math.floor(diffInSeconds / 86400)}d ago`;
    }
  };

  const spin = rotateAnim.interpolate({
    inputRange: [0, 1],
    outputRange: ['0deg', '360deg'],
  });

  if (!connected) {
    return (
      <View style={styles.container}>
        <View style={styles.bannerContainer}>
          <View style={[styles.connectedBanner, styles.disconnectedBanner]}>
            <Text style={[styles.bannerText, styles.disconnectedBannerText]}>
              📷 No Camera Feed
            </Text>
            <TouchableOpacity onPress={handlePress} style={styles.expandButton}>
              <Text style={styles.expandButtonText}>▼</Text>
            </TouchableOpacity>
          </View>
          
          {showTooltip && (
            <Animated.View style={[styles.tooltip, { opacity: fadeAnim }]}>
              <Text style={styles.tooltipText}>
                Camera is currently disconnected. Check your connection and try again.
              </Text>
            </Animated.View>
          )}
        </View>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      <View style={styles.bannerContainer}>
        <View style={styles.connectedBanner}>
          <Text style={styles.bannerText}>
            🔄 Camera Connected • Synced {getTimeAgo(lastSynced)}
          </Text>
          <TouchableOpacity onPress={handlePress} style={styles.expandButton}>
            <Text style={styles.expandButtonText}>▼</Text>
          </TouchableOpacity>
        </View>
        
        {showTooltip && (
          <Animated.View style={[styles.tooltip, { opacity: fadeAnim }]}>
            <Text style={styles.tooltipText}>
              Photos are automatically synced from the camera in real time
            </Text>
          </Animated.View>
        )}
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    paddingTop: 8,
    paddingHorizontal: 16,
    marginBottom: 12,
    zIndex: 1,
  },
  bannerContainer: {
    position: 'relative',
  },
  connectedBanner: {
    backgroundColor: '#D6EAF8',
    borderRadius: 16,
    padding: 8,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 2,
  },
  bannerText: {
    backgroundColor: '#28a745',
    color: 'white',
    paddingVertical: 4,
    paddingHorizontal: 12,
    borderRadius: 20,
    fontWeight: '600',
    fontSize: 14,
  },
  expandButton: {
    paddingLeft: 10,
  },
  expandButtonText: {
    fontSize: 16,
    color: '#666',
  },
  disconnectedBanner: {
    backgroundColor: '#FFE6E6',
  },
  disconnectedBannerText: {
    backgroundColor: '#DC3545',
  },
  badge: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 12,
    paddingVertical: 8,
    borderRadius: 20,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.2,
    shadowRadius: 4,
    elevation: 4,
    gap: 8,
  },
  connectedBadge: {
    backgroundColor: theme.colors.semantic.success,
  },
  disconnectedBadge: {
    backgroundColor: theme.colors.semantic.error,
  },
  badgeText: {
    fontSize: 13,
    fontWeight: '600',
    letterSpacing: 0.2,
  },
  connectedText: {
    color: theme.colors.surface,
  },
  disconnectedText: {
    color: theme.colors.surface,
  },
  tooltip: {
    position: 'absolute',
    top: '100%',
    left: 0,
    marginTop: 8,
    backgroundColor: 'rgba(0, 0, 0, 0.9)',
    paddingHorizontal: 12,
    paddingVertical: 8,
    borderRadius: 8,
    maxWidth: 250,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.3,
    shadowRadius: 4,
    elevation: 6,
  },
  tooltipText: {
    fontSize: 11,
    color: theme.colors.surface,
    lineHeight: 14,
    textAlign: 'center',
  },
});

export default CameraStatusBadge;
