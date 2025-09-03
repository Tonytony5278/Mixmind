import React, { useState, useEffect } from 'react';
import { Outlet } from 'react-router-dom';
import { Box, useTheme, useMediaQuery } from '@mui/material';
import { motion, AnimatePresence } from 'framer-motion';

import Sidebar from './Sidebar';
import TopBar from './TopBar';
import StatusBar from './StatusBar';
import CommandPalette from '../ui/CommandPalette';
import NotificationSystem from '../ui/NotificationSystem';

import { useAppSelector, useAppDispatch } from '../../store/hooks';
import { selectWebSocketStatus } from '../../store/slices/websocketSlice';
import { fetchPerformanceMetrics } from '../../store/slices/appSlice';

import './MainLayout.css';

const MainLayout: React.FC = () => {
  const theme = useTheme();
  const isMobile = useMediaQuery(theme.breakpoints.down('md'));
  const dispatch = useAppDispatch();
  
  const [sidebarOpen, setSidebarOpen] = useState(!isMobile);
  const [commandPaletteOpen, setCommandPaletteOpen] = useState(false);
  
  const wsStatus = useAppSelector(selectWebSocketStatus);
  
  // Performance metrics polling
  useEffect(() => {
    const interval = setInterval(() => {
      dispatch(fetchPerformanceMetrics());
    }, 10000); // Update every 10 seconds
    
    return () => clearInterval(interval);
  }, [dispatch]);
  
  // Keyboard shortcuts
  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      // Command palette (Cmd/Ctrl + K)
      if ((event.metaKey || event.ctrlKey) && event.key === 'k') {
        event.preventDefault();
        setCommandPaletteOpen(true);
      }
      
      // Toggle sidebar (Cmd/Ctrl + B)
      if ((event.metaKey || event.ctrlKey) && event.key === 'b') {
        event.preventDefault();
        setSidebarOpen(prev => !prev);
      }
    };
    
    document.addEventListener('keydown', handleKeyDown);
    return () => document.removeEventListener('keydown', handleKeyDown);
  }, []);
  
  // Auto-hide sidebar on mobile
  useEffect(() => {
    setSidebarOpen(!isMobile);
  }, [isMobile]);
  
  const sidebarWidth = 280;
  const topBarHeight = 60;
  const statusBarHeight = 24;
  
  return (
    <Box className="main-layout">
      <NotificationSystem />
      
      <CommandPalette 
        open={commandPaletteOpen}
        onClose={() => setCommandPaletteOpen(false)}
      />
      
      {/* Top Bar */}
      <Box 
        className="top-bar-container"
        sx={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          height: topBarHeight,
          zIndex: theme.zIndex.appBar,
          background: 'rgba(26, 26, 46, 0.95)',
          backdropFilter: 'blur(20px)',
          borderBottom: '1px solid rgba(255, 255, 255, 0.1)',
        }}
      >
        <TopBar 
          onToggleSidebar={() => setSidebarOpen(prev => !prev)}
          onOpenCommandPalette={() => setCommandPaletteOpen(true)}
        />
      </Box>
      
      {/* Sidebar */}
      <AnimatePresence>
        {sidebarOpen && (
          <motion.div
            initial={{ x: -sidebarWidth }}
            animate={{ x: 0 }}
            exit={{ x: -sidebarWidth }}
            transition={{ type: 'spring', stiffness: 300, damping: 30 }}
            style={{
              position: 'fixed',
              top: topBarHeight,
              left: 0,
              bottom: statusBarHeight,
              width: sidebarWidth,
              zIndex: theme.zIndex.drawer,
            }}
          >
            <Sidebar onClose={() => setSidebarOpen(false)} />
          </motion.div>
        )}
      </AnimatePresence>
      
      {/* Mobile sidebar overlay */}
      <AnimatePresence>
        {sidebarOpen && isMobile && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            transition={{ duration: 0.2 }}
            className="sidebar-overlay"
            onClick={() => setSidebarOpen(false)}
            style={{
              position: 'fixed',
              top: topBarHeight,
              left: 0,
              right: 0,
              bottom: statusBarHeight,
              background: 'rgba(0, 0, 0, 0.5)',
              zIndex: theme.zIndex.drawer - 1,
            }}
          />
        )}
      </AnimatePresence>
      
      {/* Main Content */}
      <Box
        className="main-content"
        sx={{
          marginTop: `${topBarHeight}px`,
          marginBottom: `${statusBarHeight}px`,
          marginLeft: sidebarOpen && !isMobile ? `${sidebarWidth}px` : 0,
          height: `calc(100vh - ${topBarHeight + statusBarHeight}px)`,
          transition: theme.transitions.create(['margin'], {
            easing: theme.transitions.easing.sharp,
            duration: theme.transitions.duration.enteringScreen,
          }),
          overflow: 'hidden',
          background: 'linear-gradient(135deg, #0f0f23 0%, #1a1a2e 100%)',
        }}
      >
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.4 }}
          style={{ height: '100%' }}
        >
          <Outlet />
        </motion.div>
      </Box>
      
      {/* Status Bar */}
      <Box
        className="status-bar-container"
        sx={{
          position: 'fixed',
          bottom: 0,
          left: 0,
          right: 0,
          height: statusBarHeight,
          zIndex: theme.zIndex.appBar,
          background: 'rgba(26, 26, 46, 0.95)',
          backdropFilter: 'blur(20px)',
          borderTop: '1px solid rgba(255, 255, 255, 0.1)',
        }}
      >
        <StatusBar />
      </Box>
    </Box>
  );
};

export default MainLayout;