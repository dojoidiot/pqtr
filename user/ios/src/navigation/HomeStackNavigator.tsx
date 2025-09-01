import React from 'react';
import { createNativeStackNavigator } from '@react-navigation/native-stack';
import { TouchableOpacity } from 'react-native';
import { Feather } from '@expo/vector-icons';
import HomeScreen from '../screens/HomeScreen';
import ProjectScreen from '../screens/ProjectScreen';
import ImageDetailScreen from '../screens/ImageDetailScreen';
import PresetManagerScreen from '../screens/PresetManagerScreen';
import PresetEditorScreen from '../screens/PresetEditorScreen';
import AutomationSettingsScreen from '../screens/AutomationSettingsScreen';
import AddProjectScreen from '../screens/AddProjectScreen';
import CreateProjectNavigator from './CreateProjectNavigator';
import { theme } from '../constants/theme';

export type HomeStackParamList = {
  Home: undefined;
  Project: {
    projectTitle: string;
    projectId: string;
  };
  ImageDetail: {
    id: string;
    projectId?: string;
    imageUrl?: string;
    imageTitle?: string;
  };
  AutomationSettings: {
    projectId: string;
  };
  PresetManager: undefined;
  PresetEditor: {
    mode: 'create' | 'edit';
    preset?: any;
  };
  AddProject: undefined;
  CreateProject: undefined;
};

const Stack = createNativeStackNavigator<HomeStackParamList>();

const HomeStackNavigator = () => {
  return (
    <Stack.Navigator
      initialRouteName="Home"
      screenOptions={{
        headerShown: false,
      }}
    >
      <Stack.Screen name="Home" component={HomeScreen} />
      <Stack.Screen
        name="AddProject"
        component={AddProjectScreen}
        options={{
          headerShown: false,
        }}
      />
      <Stack.Screen
        name="CreateProject"
        component={CreateProjectNavigator}
        options={{
          headerShown: false,
        }}
      />
      <Stack.Screen
        name="Project"
        component={ProjectScreen}
        options={{
          headerShown: false,
        }}
      />
      <Stack.Screen
        name="ImageDetail"
        component={ImageDetailScreen}
        options={{
          headerShown: false,
        }}
      />
      <Stack.Screen
        name="PresetManager"
        component={PresetManagerScreen}
        options={{
          headerShown: false,
        }}
      />
      <Stack.Screen
        name="PresetEditor"
        component={PresetEditorScreen}
        options={{
          headerShown: false,
        }}
      />
      <Stack.Screen
        name="AutomationSettings"
        component={AutomationSettingsScreen}
        options={{
          headerShown: false,
        }}
      />
    </Stack.Navigator>
  );
};

export default HomeStackNavigator; 