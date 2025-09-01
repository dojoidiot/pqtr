// ----------------------
// STEP 1: Project Name
// ----------------------

import React, { useState, useEffect } from 'react';
import {
  View,
  Text,
  TextInput,
  StyleSheet,
  KeyboardAvoidingView,
  Platform,
  SafeAreaView,
} from 'react-native';
import { useNavigation } from '@react-navigation/native';
import { Ionicons } from '@expo/vector-icons';
import HeaderWithBack from '../../components/create-project/HeaderWithBack';
import StepFooter from '../../components/create-project/StepFooter';

export default function ProjectNameScreen() {
  const navigation = useNavigation();
  const [projectName, setProjectName] = useState('');
  const [isInputFocused, setIsInputFocused] = useState(false);

  const handleNext = () => {
    if (projectName.trim()) {
      // @ts-ignore - Navigation type will be properly typed when integrated
      navigation.navigate('SelectCamera', { projectName: projectName.trim() });
    }
  };

  return (
    <SafeAreaView style={styles.container}>
      <KeyboardAvoidingView 
        style={styles.keyboardView}
        behavior={Platform.OS === 'ios' ? 'padding' : 'height'}
      >
        {/* Header with Logo */}
        <View style={styles.logoContainer}>
          <Text style={styles.logo}>PQTR</Text>
        </View>

        {/* Main Content */}
        <View style={styles.content}>
          <HeaderWithBack 
            title="New Project" 
            showBack={false}
          />
          
          <View style={styles.inputContainer}>
            <Text style={styles.inputLabel}>Project Name</Text>
            <TextInput
              style={[
                styles.input,
                isInputFocused && styles.inputFocused
              ]}
              placeholder="Australian Grand Prix"
              placeholderTextColor="#AAA"
              value={projectName}
              onChangeText={setProjectName}
              onFocus={() => setIsInputFocused(true)}
              onBlur={() => setIsInputFocused(false)}
              autoFocus
              autoCapitalize="words"
              returnKeyType="done"
              onSubmitEditing={handleNext}
            />
            
            <View style={styles.hintContainer}>
              <Ionicons name="information-circle-outline" size={16} color="#999" />
              <Text style={styles.hintText}>
                This will be the name of your photo collection
              </Text>
            </View>
          </View>
        </View>

        {/* Footer */}
        <StepFooter
          buttonText="Next"
          onPress={handleNext}
          disabled={!projectName.trim()}
        />
      </KeyboardAvoidingView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#F9F6F1',
  },
  keyboardView: {
    flex: 1,
  },
  logoContainer: {
    alignItems: 'center',
    paddingTop: 20,
    paddingBottom: 10,
  },
  logo: {
    fontSize: 24,
    fontWeight: '700',
    color: '#222',
    letterSpacing: 2,
  },
  content: {
    flex: 1,
    paddingHorizontal: 24,
  },
  inputContainer: {
    marginTop: 40,
  },
  inputLabel: {
    fontSize: 18,
    fontWeight: '600',
    color: '#222',
    marginBottom: 12,
  },
  input: {
    backgroundColor: '#FFF',
    borderRadius: 16,
    paddingVertical: 18,
    paddingHorizontal: 20,
    fontSize: 18,
    color: '#222',
    borderWidth: 2,
    borderColor: '#F0F0F0',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.05,
    shadowRadius: 8,
    elevation: 2,
  },
  inputFocused: {
    borderColor: '#175E4C',
    shadowColor: '#175E4C',
    shadowOpacity: 0.1,
    shadowRadius: 12,
    elevation: 4,
  },
  hintContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    marginTop: 12,
    paddingHorizontal: 4,
  },
  hintText: {
    fontSize: 14,
    color: '#999',
    marginLeft: 8,
    lineHeight: 20,
  },
});
