import React from 'react';
import { Box, Typography, LinearProgress } from '@mui/material';
import { motion } from 'framer-motion';

interface LoadingScreenProps {
  message?: string;
  progress?: number;
}

const LoadingScreen: React.FC<LoadingScreenProps> = ({ 
  message = 'Loading MixMind AI...', 
  progress 
}) => {
  return (
    <Box
      sx={{
        height: '100vh',
        width: '100vw',
        display: 'flex',
        flexDirection: 'column',
        justifyContent: 'center',
        alignItems: 'center',
        background: 'linear-gradient(135deg, #0f0f23 0%, #1a1a2e 100%)',
        color: '#e1e1e6',
      }}
    >
      <motion.div
        initial={{ opacity: 0, scale: 0.8 }}
        animate={{ opacity: 1, scale: 1 }}
        transition={{ duration: 0.5 }}
        style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          gap: '2rem',
        }}
      >
        {/* Logo Animation */}
        <motion.div
          animate={{
            rotate: [0, 360],
          }}
          transition={{
            duration: 2,
            repeat: Infinity,
            ease: "linear",
          }}
          style={{
            width: 80,
            height: 80,
            borderRadius: '50%',
            background: 'linear-gradient(135deg, #6366f1 0%, #7c3aed 50%, #ec4899 100%)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            position: 'relative',
            overflow: 'hidden',
          }}
        >
          <motion.div
            animate={{
              scale: [1, 1.2, 1],
              opacity: [0.7, 1, 0.7],
            }}
            transition={{
              duration: 1.5,
              repeat: Infinity,
              ease: "easeInOut",
            }}
            style={{
              width: 60,
              height: 60,
              borderRadius: '50%',
              background: 'rgba(255, 255, 255, 0.2)',
              backdropFilter: 'blur(10px)',
            }}
          />
        </motion.div>
        
        {/* Brand Name */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ delay: 0.3, duration: 0.5 }}
        >
          <Typography
            variant="h3"
            sx={{
              fontWeight: 700,
              background: 'linear-gradient(135deg, #6366f1 0%, #7c3aed 50%, #ec4899 100%)',
              backgroundClip: 'text',
              WebkitBackgroundClip: 'text',
              WebkitTextFillColor: 'transparent',
              textAlign: 'center',
              marginBottom: 1,
            }}
          >
            MixMind AI
          </Typography>
          <Typography
            variant="subtitle1"
            sx={{
              color: '#a1a1aa',
              textAlign: 'center',
              fontWeight: 400,
            }}
          >
            Advanced DAW with AI-powered mixing
          </Typography>
        </motion.div>
        
        {/* Loading Message */}
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          transition={{ delay: 0.6, duration: 0.5 }}
          style={{ textAlign: 'center', minHeight: '1.5rem' }}
        >
          <Typography
            variant="body1"
            sx={{
              color: '#e1e1e6',
              marginBottom: 2,
              fontWeight: 500,
            }}
          >
            {message}
          </Typography>
          
          {/* Progress Bar */}
          {progress !== undefined ? (
            <Box sx={{ width: 300, marginTop: 2 }}>
              <LinearProgress
                variant="determinate"
                value={progress}
                sx={{
                  height: 4,
                  borderRadius: 2,
                  backgroundColor: 'rgba(255, 255, 255, 0.1)',
                  '& .MuiLinearProgress-bar': {
                    background: 'linear-gradient(135deg, #6366f1 0%, #7c3aed 100%)',
                    borderRadius: 2,
                  },
                }}
              />
              <Typography
                variant="caption"
                sx={{
                  color: '#a1a1aa',
                  display: 'block',
                  textAlign: 'center',
                  marginTop: 1,
                }}
              >
                {Math.round(progress)}%
              </Typography>
            </Box>
          ) : (
            <Box sx={{ width: 300, marginTop: 2 }}>
              <LinearProgress
                sx={{
                  height: 4,
                  borderRadius: 2,
                  backgroundColor: 'rgba(255, 255, 255, 0.1)',
                  '& .MuiLinearProgress-bar': {
                    background: 'linear-gradient(135deg, #6366f1 0%, #7c3aed 100%)',
                    borderRadius: 2,
                  },
                }}
              />
            </Box>
          )}
        </motion.div>
        
        {/* Animated Dots */}
        <motion.div
          style={{
            display: 'flex',
            gap: '0.5rem',
            marginTop: '1rem',
          }}
        >
          {[0, 1, 2].map((index) => (
            <motion.div
              key={index}
              animate={{
                scale: [1, 1.5, 1],
                opacity: [0.3, 1, 0.3],
              }}
              transition={{
                duration: 1.5,
                repeat: Infinity,
                delay: index * 0.2,
                ease: "easeInOut",
              }}
              style={{
                width: 8,
                height: 8,
                borderRadius: '50%',
                backgroundColor: '#6366f1',
              }}
            />
          ))}
        </motion.div>
        
        {/* System Status */}
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          transition={{ delay: 1, duration: 0.5 }}
          style={{
            marginTop: '2rem',
            textAlign: 'center',
            color: '#6b7280',
            fontSize: '0.75rem',
          }}
        >
          <Typography variant="caption">
            Initializing audio engine and AI services...
          </Typography>
        </motion.div>
      </motion.div>
    </Box>
  );
};

export default LoadingScreen;