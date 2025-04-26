
      <h1>SMART IRRIGATION SYSTEM - v2</h1>
      <div class="datetime">
        <p id="date">Loading date...</p>
        <p id="time">Loading time...</p>
      </div>
    
      <div class="status">
        <p>[RUN] : SYSTEM Status : <span id="mcbStatus">Unknown</span></p>
        <p>[TIME] : Total Duration: <span id="duration">Unknown</span></p>
        <p>[WiFi] : SSID: <span id="ssid">Unknown</span></p>
      </div>
    
      <form action="/manualOn" method="POST">
        <input type="submit" value="Manually ON">
      </form>
      <form action="/manualOff" method="POST">
        <input type="submit" value="Manually OFF">
      </form>
      <form action="/daily" method="POST">
        <input type="submit" value="Set Daily Timer">
      </form>
      <form action="/weekly" method="POST">
        <input type="submit" value="Set Weekly Schedule">
      </form>
      <form action="/logout" method="POST">
        <input type="submit" value="Logout">
      </form>
      <form action="/emergency" method="POST">
        <input type="submit" value="Shut-Down!" style="color: red; font-weight: bold;">
      </form>
    
      <script>
        function updateStatus() {
          fetch('/status')
            .then(response => response.json())
            .then(data => {
              document.getElementById('mcbStatus').textContent = data.mcbStatus ? 'NORMAL' : 'TRIGGER';
            })
            .catch(error => console.error('Error fetching status:', error));
        }
    
        function updateDuration() {
          fetch('/duration')
            .then(response => response.json())
            .then(data => {
              document.getElementById('duration').textContent = data.duration;
            })
            .catch(error => console.error('Error fetching duration:', error));
        }
    
        function updateDateTime() {
          fetch('/dateTime')
            .then(response => response.json())
            .then(data => {
              document.getElementById('date').textContent = data.date;
              document.getElementById('time').textContent = data.time;
            })
            .catch(error => console.error('Error fetching date/time:', error));
        }
    
        function updateWifiInfo() {
          fetch('/wifiInfo')
            .then(response => response.json())
            .then(data => {
              document.getElementById('ssid').textContent = data.ssid;
            })
            .catch(error => console.error('Error fetching WiFi info:', error));
        }
       
        setInterval(updateStatus, 1000);
        setInterval(updateDuration, 60000);
        setInterval(updateDateTime, 1000);
        setInterval(updateWifiInfo, 10000);
    
        updateStatus();
        updateDuration();
        updateDateTime();
        updateWifiInfo();
    
    
      </script>
    
      <!-- Scrollable Days Section -->
      <div class="scrollable-days">
        <label for="daySelector">Select a Day:</label>
        <select id="daySelector" onchange="updateTimes(this.value)">
          <option value="0">Sunday</option>
          <option value="1">Monday</option>
          <option value="2">Tuesday</option>
          <option value="3">Wednesday</option>
          <option value="4">Thursday</option>
          <option value="5">Friday</option>
          <option value="6">Saturday</option>
        
        </select>
    
        <div id="timeInputs">
      <h2>SET TIME</h2>
      <label for="onTime">On Time (HH:MM): </label>
      <span id="onTime" name="onTime">00:00</span><br>
      <label for="offTime">Off Time (HH:MM): </label>
      <span id="offTime" name="offTime">00:00</span><br>
    </div>
    
      <!-- End of Scrollable Days Section -->
    
      <div class="footer">
        ⚡ Embedded & IoT Products Developments [R&D]  V_3.0.0-M2 (Esp8266) SN:ABLZx001  ID164 EEE ⚡  [Issued: 01/10/2024]
      </div>
      
       <script>
     function updateSchedule() {
        const newSchedule = [
            { onHour: 5, onMinute: 0, offHour: 0, offMinute: 0 },
            { onHour: 0, onMinute: 0, offHour: 9, offMinute: 35 },
            { onHour: 9, onMinute: 30, offHour: 9, offMinute: 35 },
            { onHour: 9, onMinute: 30, offHour: 9, offMinute: 35 },
            { onHour: 9, onMinute: 30, offHour: 9, offMinute: 35 },
            { onHour: 9, onMinute: 30, offHour: 9, offMinute: 35 },
            { onHour: 9, onMinute: 30, offHour: 9, offMinute: 35 }
        ];
    
        fetch('http://<your_device_ip>/schedule', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(newSchedule)
        })
        .then(response => response.json())
        .then(data => {
            console.log('Success:', data);
        })
        .catch((error) => {
            console.error('Error:', error);
        });
    }
    
    // Call this function to update the schedule
    updateSchedule();
    
      </script>
      
    </body>
    </html>
    )rawliteral";