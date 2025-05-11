// Configuration
const config = {
  pressure: {
    min: 2,
    max: 4,
    warningThreshold: 3.5,
    dangerThreshold: 4
  },
  temperature: {
    min: 0,
    max: 50,
    warningThreshold: 40,
    dangerThreshold: 45
  },
  flow: {
    min: 0,
    max: 40
  }
};

// DOM Elements
const elements = {
  pressure: document.getElementById('pressure'),
  temperature: document.getElementById('temperature'),
  flow: document.getElementById('flow'),
  motor: document.getElementById('motor'),
  mode: document.getElementById('mode'),
  pressureBar: document.getElementById('pressureBar'),
  wifiStatus: document.getElementById('wifiStatus'),
  ipAddress: document.getElementById('ipAddress'),
  toggleBtn: document.getElementById('toggleBtn'),
  overrideBtn: document.getElementById('overrideBtn')
};

let eventSource;
let isConnected = false;

function connectSSE() {
  if (eventSource) {
    eventSource.close();
  }

  eventSource = new EventSource('/events');
  
  eventSource.onopen = () => {
    console.log('SSE connection opened');
    isConnected = true;
    updateConnectionStatus(true);
  };

  eventSource.onerror = (e) => {
    console.log('SSE error:', e);
    isConnected = false;
    updateConnectionStatus(false);
    
    // Close the connection before attempting to reconnect
    eventSource.close();
    setTimeout(connectSSE, 3000);
  };

  eventSource.addEventListener('update', (e) => {
    try {
      const data = JSON.parse(e.data);
      updateDashboard(data);
    } catch (err) {
      console.error('Error parsing SSE data:', err);
    }
  });
}
function updateDashboard(data) {
  if (data.pressure !== undefined) {
    elements.pressure.textContent = data.pressure.toFixed(2);
    updatePressureBar(data.pressure);
  }
  
  if (data.temperature !== undefined) {
    elements.temperature.textContent = data.temperature.toFixed(2);
    updateTemperatureColor(data.temperature);
  }
  
  if (data.flow !== undefined) {
    elements.flow.textContent = data.flow.toFixed(2);
  }
  
  if (data.motor !== undefined) {
    elements.motor.textContent = data.motor ? "ON" : "OFF";
    elements.motor.dataset.status = data.motor ? "ON" : "OFF";
  }
  
  if (data.manualOverride !== undefined) {
    elements.mode.textContent = data.manualOverride ? "MANUAL" : "AUTO";
    elements.mode.dataset.status = data.manualOverride ? "MANUAL" : "AUTO";
    elements.overrideBtn.style.backgroundColor = 
      data.manualOverride ? 'var(--warning)' : 'var(--secondary)';
  }
}

function updatePressureBar(pressure) {
  const percentage = Math.min(100, (pressure / config.pressure.max) * 100);
  elements.pressureBar.style.setProperty('--width', `${percentage}%`);
  
  if (pressure >= config.pressure.dangerThreshold) {
    elements.pressureBar.style.setProperty('--color', 'var(--danger)');
  } else if (pressure >= config.pressure.warningThreshold) {
    elements.pressureBar.style.setProperty('--color', 'var(--warning)');
  } else {
    elements.pressureBar.style.setProperty('--color', 'var(--secondary)');
  }
}

function updateTemperatureColor(temp) {
  if (temp >= config.temperature.dangerThreshold) {
    elements.temperature.style.color = 'var(--danger)';
  } else if (temp >= config.temperature.warningThreshold) {
    elements.temperature.style.color = 'var(--warning)';
  } else {
    elements.temperature.style.color = 'var(--secondary)';
  }
}

function updateConnectionStatus(connected) {
  const wifiIcon = elements.wifiStatus.querySelector('i');
  const statusText = connected ? window.location.hostname : 'Disconnected';
  
  wifiIcon.className = connected ? 'fas fa-wifi' : 'fas fa-wifi-slash';
  wifiIcon.style.color = connected ? '#4bb543' : '#ec0b43';
  elements.ipAddress.textContent = statusText;
  
  elements.toggleBtn.disabled = !connected;
  elements.overrideBtn.disabled = !connected;
}

function sendCommand(command) {
  const validCommands = ['toggle', 'override'];
  if (!validCommands.includes(command)) {
    console.error('Invalid command:', command);
    return;
  }
  
  if (!isConnected) {
    console.error('Cannot send command: not connected to server');
    return;
  }
  
  fetch('/command', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({ command })
  })
  .then(response => {
    if (!response.ok) {
      throw new Error(`Server returned ${response.status}: ${response.statusText}`);
    }
    return response.json();
  })
  .then(data => console.log('Command response:', data))
  .catch(error => {
    console.error('Error sending command:', error);
    // You could add UI feedback here
  });
}

function rebootDevice() {
    fetch('/reboot', {
        method: 'POST'
    })
    .then(response => response.json())
    .then(data => {
        console.log('Reboot initiated');
        alert('Device will reboot shortly');
    });
}

// Initialize when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
  connectSSE();
  
  // Set initial styles
  elements.pressureBar.style.setProperty('--width', '0%');
  elements.pressureBar.style.setProperty('--color', 'var(--secondary)');
  
  // Event listeners
  elements.toggleBtn.addEventListener('click', () => {
    sendCommand('toggle');
  });

  elements.overrideBtn.addEventListener('click', () => {
    sendCommand('override');
  });
});