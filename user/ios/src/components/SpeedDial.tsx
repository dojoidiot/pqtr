import React, { useState, useRef } from 'react';
import { 
  View, 
  TouchableOpacity, 
  Text, 
  StyleSheet, 
  Animated,
  Dimensions 
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import * as Haptics from 'expo-haptics';
import { theme } from '../constants/theme';

interface SpeedDialAction {
  icon: string;
  label: string;
  onPress: () => void;
  color?: string;
}

interface SpeedDialProps {
  isOpen: boolean;
  icon: React.ReactNode;
  actions: SpeedDialAction[];
  onToggle: () => void;
}

const { width: screenWidth } = Dimensions.get('window');

const SpeedDial: React.FC<SpeedDialProps> = ({ 
  isOpen, 
  icon, 
  actions, 
  onToggle 
}) => {
  const [isAnimating, setIsAnimating] = useState(false);
  const scaleAnim = useRef(new Animated.Value(1)).current;
  const actionAnims = useRef(actions.map(() => new Animated.Value(0))).current;

  const handleToggle = () => {
    if (isAnimating) return;
    
    setIsAnimating(true);
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium);
    
    if (isOpen) {
      // Close animation
      Animated.parallel([
        Animated.spring(scaleAnim, {
          toValue: 1,
          useNativeDriver: true,
          tension: 100,
          friction: 8,
        }),
        ...actionAnims.map(anim => 
          Animated.timing(anim, {
            toValue: 0,
            duration: 200,
            useNativeDriver: true,
          })
        ),
      ]).start(() => {
        setIsAnimating(false);
        onToggle();
      });
    } else {
      // Open animation
      onToggle();
      Animated.parallel([
        Animated.spring(scaleAnim, {
          toValue: 0.9,
          useNativeDriver: true,
          tension: 100,
          friction: 8,
        }),
        ...actionAnims.map((anim, index) => 
          Animated.timing(anim, {
            toValue: 1,
            duration: 300,
            delay: index * 50,
            useNativeDriver: true,
          })
        ),
      ]).start(() => {
        setIsAnimating(false);
      });
    }
  };

  const handleActionPress = (action: SpeedDialAction, index: number) => {
    Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light);
    action.onPress();
  };

  return (
    <View style={styles.container}>
      {/* Action Buttons */}
      {isOpen && actions.map((action, index) => (
        <Animated.View
          key={action.label}
                        style={[
                styles.actionButton,
                {
                  transform: [
                    { 
                      translateY: actionAnims[index].interpolate({
                        inputRange: [0, 1],
                        outputRange: [20, -((index + 1) * 70)],
                      })
                    },
                    { 
                      scale: actionAnims[index].interpolate({
                        inputRange: [0, 1],
                        outputRange: [0.5, 1],
                      })
                    }
                  ],
                  opacity: actionAnims[index],
                },
              ]}
        >
          <TouchableOpacity
            style={[
              styles.actionTouchable,
              { backgroundColor: action.color || theme.colors.surface }
            ]}
            onPress={() => handleActionPress(action, index)}
            activeOpacity={0.8}
            accessibilityLabel={action.label}
            accessibilityRole="button"
          >
            <Ionicons 
              name={action.icon as any} 
              size={20} 
              color={theme.colors.text.primary} 
            />
          </TouchableOpacity>
          <Text style={styles.actionLabel}>{action.label}</Text>
        </Animated.View>
      ))}
      
      {/* Main FAB */}
      <Animated.View style={[styles.fab, { transform: [{ scale: scaleAnim }] }]}>
        <TouchableOpacity
          style={styles.fabTouchable}
          onPress={handleToggle}
          activeOpacity={0.8}
          accessibilityLabel={isOpen ? "Close menu" : "Open menu"}
          accessibilityRole="button"
        >
          <Animated.View style={{
            transform: [{
              rotate: isOpen ? '45deg' : '0deg'
            }]
          }}>
            {icon}
          </Animated.View>
        </TouchableOpacity>
      </Animated.View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    position: 'absolute',
    bottom: 100,
    right: theme.spacing.l,
    alignItems: 'center',
  },
  fab: {
    width: 56,
    height: 56,
    borderRadius: 28,
    backgroundColor: theme.colors.green,
    justifyContent: 'center',
    alignItems: 'center',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.3,
    shadowRadius: 8,
    elevation: 8,
  },
  fabTouchable: {
    width: '100%',
    height: '100%',
    justifyContent: 'center',
    alignItems: 'center',
  },
  actionButton: {
    alignItems: 'center',
    marginBottom: theme.spacing.s,
  },
  actionTouchable: {
    width: 48,
    height: 48,
    borderRadius: 24,
    justifyContent: 'center',
    alignItems: 'center',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.2,
    shadowRadius: 4,
    elevation: 4,
  },
  actionLabel: {
    fontSize: 12,
    fontWeight: '500',
    color: theme.colors.text.secondary,
    marginTop: 4,
    textAlign: 'center',
    maxWidth: 80,
  },
});

export default SpeedDial;
