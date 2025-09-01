import React, { useRef } from 'react';
import { TouchableOpacity, Text, Animated, StyleSheet } from 'react-native';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

interface TagFilterChipProps {
  label: string;
  active: boolean;
  onPress: () => void;
}

const TagFilterChip: React.FC<TagFilterChipProps> = ({ label, active, onPress }) => {
  const scaleAnim = useRef(new Animated.Value(1)).current;

  const handlePress = () => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    onPress();
  };

  const handlePressIn = () => {
    Animated.spring(scaleAnim, {
      toValue: 0.95,
      useNativeDriver: true,
      tension: 100,
      friction: 8,
    }).start();
  };

  const handlePressOut = () => {
    Animated.spring(scaleAnim, {
      toValue: 1,
      useNativeDriver: true,
      tension: 100,
      friction: 8,
    }).start();
  };

  return (
    <Animated.View style={{ transform: [{ scale: scaleAnim }] }}>
      <TouchableOpacity
        style={[
          styles.chip,
          active ? styles.chipActive : styles.chipInactive
        ]}
        onPress={handlePress}
        onPressIn={handlePressIn}
        onPressOut={handlePressOut}
        activeOpacity={0.8}
        accessibilityRole="button"
        accessibilityLabel={`Filter by ${label} tag`}
        accessibilityState={{ selected: active }}
      >
        <Text style={[
          styles.chipText,
          active ? styles.chipTextActive : styles.chipTextInactive
        ]}>
          {label}
        </Text>
      </TouchableOpacity>
    </Animated.View>
  );
};

const styles = StyleSheet.create({
  chip: {
    paddingHorizontal: 16,
    paddingVertical: 10,
    borderRadius: 20,
    marginRight: 12,
    minHeight: 44,
    minWidth: 44,
    justifyContent: 'center',
    alignItems: 'center',
    borderWidth: 1,
  },
  chipActive: {
    backgroundColor: theme.colors.semantic.primary,
    borderColor: theme.colors.semantic.primary,
    shadowColor: theme.colors.semantic.primary,
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.3,
    shadowRadius: 4,
    elevation: 4,
  },
  chipInactive: {
    backgroundColor: theme.colors.surface,
    borderColor: 'rgba(0, 0, 0, 0.12)',
  },
  chipText: {
    fontSize: 13,
    fontWeight: '600',
    letterSpacing: 0.2,
  },
  chipTextActive: {
    color: theme.colors.surface,
  },
  chipTextInactive: {
    color: theme.colors.text.primary,
  },
});

export default TagFilterChip;
