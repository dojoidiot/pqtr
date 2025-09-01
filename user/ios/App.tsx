import React from 'react';
import { StatusBar } from 'expo-status-bar';
import AppNavigator from './src/navigation/AppNavigator';
import { PresetProvider } from './src/context/PresetContext';

export default function App() {
  return (
    <PresetProvider>
      <StatusBar style="dark" />
      <AppNavigator />
    </PresetProvider>
  );
}
