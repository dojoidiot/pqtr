import React from 'react';
import { View, Text, TouchableOpacity, StyleSheet } from 'react-native';
import Animated, { 
  useSharedValue, 
  useAnimatedStyle, 
  withSpring,
  withSequence,
  withTiming 
} from 'react-native-reanimated';

interface StepFooterProps {
  buttonText: string;
  onPress: () => void;
  disabled?: boolean;
  isFinal?: boolean;
}

export default function StepFooter({ buttonText, onPress, disabled = false, isFinal = false }: StepFooterProps) {
  const scale = useSharedValue(1);

  const handlePress = () => {
    if (disabled) return;
    
    // Microinteraction: subtle pulse animation
    scale.value = withSequence(
      withTiming(0.95, { duration: 100 }),
      withSpring(1, { damping: 15, stiffness: 300 })
    );
    
    onPress();
  };

  const animatedStyle = useAnimatedStyle(() => ({
    transform: [{ scale: scale.value }],
  }));

  return (
    <View style={styles.footer}>
      <Animated.View style={[styles.buttonContainer, animatedStyle]}>
        <TouchableOpacity
          style={[
            styles.button,
            disabled && styles.buttonDisabled,
            isFinal && styles.buttonFinal
          ]}
          onPress={handlePress}
          disabled={disabled}
          activeOpacity={0.8}
        >
          <Text style={[
            styles.buttonText,
            disabled && styles.buttonTextDisabled,
            isFinal && styles.buttonTextFinal
          ]}>
            {buttonText}
          </Text>
        </TouchableOpacity>
      </Animated.View>
    </View>
  );
}

const styles = StyleSheet.create({
  footer: {
    position: 'absolute',
    bottom: 0,
    left: 0,
    right: 0,
    backgroundColor: '#F9F6F1',
    paddingHorizontal: 24,
    paddingTop: 20,
    paddingBottom: 40,
    borderTopWidth: 1,
    borderTopColor: '#F0F0F0',
  },
  buttonContainer: {
    width: '100%',
  },
  button: {
    backgroundColor: '#222',
    paddingVertical: 18,
    borderRadius: 16,
    alignItems: 'center',
    justifyContent: 'center',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 8,
    elevation: 4,
  },
  buttonFinal: {
    backgroundColor: '#175E4C',
  },
  buttonDisabled: {
    backgroundColor: '#E0E0E0',
    shadowOpacity: 0,
    elevation: 0,
  },
  buttonText: {
    color: '#FFF',
    fontSize: 18,
    fontWeight: '600',
  },
  buttonTextFinal: {
    color: '#FFF',
  },
  buttonTextDisabled: {
    color: '#999',
  },
});
