import SwiftUI

struct ContentView: View {
    
    // let myArduinoIPAddress = "192.168.0.238"     // Arduino Nano WiFi Rev2
    // let myArduinoIPAddress = "192.168.0.32"         // Arduino Nano ESP32
    let myArduinoIPAddress = "192.168.0.70"         // spare Arduino Nano ESP32
    let HEARTBEAT_INTERVAL: Double = 15
    
    @State private var month = 1
    @State private var day = 1
    @State private var year = 2024
    @State private var hour = 0
    @State private var minute = 0
    @State private var second = 0
    @State private var numberOfZones = 3
    @State private var duration = 30
    @State private var schedule: [SprinklerSchedule] = []
    @State private var params: [String: Any] = [:]
    @State private var responseMessage: String = "Waiting for response..."
    @State private var isSprinklerOn: Bool = false // State to track if the sprinkler is on
    @State private var blinkOpacity: Double = 1.0 // for the blinking effect on "Sprinkler is ON"
    @State private var showAlert = false  // to limit number of schedule entries
    @State private var timer: Timer? = nil // to send "HI!" command every 60 seconds
    @State private var activeDaysBitfield: Int = 0 // New state variable for the bitfield
    
    @State private var badSchedule: Bool = false // State to track if the sprinkler is on
    @State private var badHeapMem_: Bool = false // State to track if the sprinkler is on
    @State private var badTimeStmp: Bool = false // State to track if the sprinkler is on
    @State private var badRSSIsign: Bool = false // State to track if the sprinkler is on
    @State private var badCommand_: Bool = false // State to track if the sprinkler is on
    
    var body: some View {
        
        VStack {
            // Sprinkler ON LED
            HStack {
                Circle()
                    .fill(isSprinklerOn ? Color.green : Color.gray)
                    .frame(width: 10, height: 10)
                Text(isSprinklerOn ? "Sprinkler is ON" : "Sprinkler is OFF")
                    .foregroundColor(isSprinklerOn ? Color.green : Color.gray)
                    .opacity(isSprinklerOn && blinkOpacity == 0.0 ? 0.0 : 1.0) // Control opacity based on blinking state
                    .animation(isSprinklerOn ? .easeInOut(duration: 0.5).repeatForever(autoreverses: true) : .none, value: blinkOpacity)  // Start/stop animation based on sprinkler state
            }
            //.padding()
            
           
            HStack {
                Button("ON") {
                    sendCommand(command: "ONN", httpMethod: "GET", params: [:])
                }
                .padding(5)
                .frame(width: 50, height: 30) // Set a fixed width and height
                .background(Color.green)
                .foregroundColor(.white)
                .cornerRadius(10)
                .shadow(radius: 5)
                
                Button("OFF") {
                    sendCommand(command: "OFF", httpMethod: "GET", params: [:])
                }
                .padding(5)
                .frame(width: 50, height: 30) // Set a fixed width and height
                .background(Color.gray)
                .foregroundColor(.white)
                .cornerRadius(10)
                .shadow(radius: 5)
            }
            
            // Schedule settings and entries inside a bounding box
            GroupBox(label: Label("Schedule Settings", systemImage: "calendar")) {
                // Day LEDs
                HStack {
                    ForEach(0..<7) { index in
                        VStack {
                            Circle()
                                .fill((activeDaysBitfield & (1 << index)) != 0 ? Color.green : Color.gray)
                                .frame(width: 10, height: 10)
                            Text(dayShortName(for: index))
                                .font(.caption2)
                        }
                    }
                }
                //.padding()
     
                VStack(spacing: 8) {
                    Stepper("Number of Zones: \(numberOfZones)", value: $numberOfZones, in: 1...4)
                        .padding([.top, .horizontal])

                    Stepper("Duration per Zone: \(duration) min", value: $duration, in: 0...60)
                        .padding([.horizontal, .bottom])

                    Divider() // Divider between steppers and schedule entries
                        .padding(.bottom, 4) // Reduce bottom padding to bring it closer to the List

                    List {
                        ForEach(schedule) { entry in
                            HStack {
                                Picker("Day of the Week", selection: Binding(
                                    get: { entry.dayOfWeek },
                                    set: { newValue in
                                        if let index = schedule.firstIndex(where: { $0.id == entry.id }) {
                                            schedule[index].dayOfWeek = newValue
                                        }
                                    }
                                )) {
                                    ForEach(0..<7) { day in
                                        Text(dayName(for: day)).tag(day)
                                    }
                                }
                                .pickerStyle(MenuPickerStyle()) // Use a compact style for the Picker
                                .frame(maxWidth: .infinity) // Ensure Picker uses available space
                                
                                DatePicker("Time", selection: Binding(
                                    get: { entry.time },
                                    set: { newValue in
                                        if let index = schedule.firstIndex(where: { $0.id == entry.id }) {
                                            schedule[index].time = newValue
                                        }
                                    }
                                ), displayedComponents: [.hourAndMinute])
                                .labelsHidden() // Hide labels to make it more compact
                                .frame(maxWidth: .infinity) // Ensure DatePicker uses available space
                            }
                        }
                        .onDelete { indexSet in
                            schedule.remove(atOffsets: indexSet)
                        }
                    }
                    .frame(maxHeight: 200) // Set a maximum height for the List to ensure visibility
                    .padding(.top, 0) // Remove top padding from the List if it's adding extra space
                }
                //.padding()
            }
            

            //.padding()

            HStack {
                Button("Add") {
                    if schedule.count < 2 {
                        schedule.append(SprinklerSchedule(dayOfWeek: 0, time: Date()))
                    } else {
                        showAlert = true // Trigger the alert
                    }
                }
                .padding(5)
                .frame(width: 50, height: 30) // Set a fixed width and height
                .background(Color.gray)
                .foregroundColor(.white)
                .cornerRadius(10)
                .shadow(radius: 5)
                .alert(isPresented: $showAlert) {
                    Alert(title: Text("Limit Reached"), message: Text("You can only add up to 2 schedules."), dismissButton: .default(Text("OK")))
                }
                
                Button("Send") {
                    sendSetScheduleCommand()
                }
                .padding(5)
                .frame(width: 50, height: 30) // Set a fixed width and height
                .background(Color.gray)
                .foregroundColor(.white)
                .cornerRadius(10)
                .shadow(radius: 5)
            }
            GroupBox(label: Label("uController status", systemImage: "exclamationmark.triangle")) {
                // Day LEDs
                HStack {}
            }
            
        }
        HStack {
            Circle()
                .fill(badSchedule ? Color.red : Color.green)
                .frame(width: 10, height: 10)
            Text("Sched")
                .foregroundColor(Color.black)
                .opacity(1.0)
                //.animation(isSprinklerOn ? .easeInOut(duration: 0.5).repeatForever(autoreverses: true) : .none, value: blinkOpacity)  // Start/stop animation based on sprinkler state
            Circle()
                .fill(badHeapMem_ ? Color.red : Color.green)
                .frame(width: 10, height: 10)
            Text("Heap")
                .foregroundColor(Color.black)
                .opacity(1.0)
            Circle()
                .fill(badTimeStmp ? Color.red : Color.green)
                .frame(width: 10, height: 10)
            Text("Time")
                .foregroundColor(Color.black)
                .opacity(1.0)
            Circle()
                .fill(badRSSIsign ? Color.red : Color.green)
                .frame(width: 10, height: 10)
            Text("RSSI")
                .foregroundColor(Color.black)
                .opacity(1.0)
            Circle()
                .fill(badCommand_ ? Color.red : Color.green)
                .frame(width: 10, height: 10)
            Text("Cmd")
                .foregroundColor(Color.black)
                .opacity(1.0)
        }
        
        .padding(.top, -20) // Use a negative top padding to pull content up
        
        .onAppear {
            // Send the "HI!" command when ContentView appears
            sendCommand(command: "HI!", httpMethod: "GET", params: [:])
            if isSprinklerOn {
                startBlinking()
            }
            startTimer()
        }
        .onDisappear {
            stopTimer()
        }
        .onChange(of: isSprinklerOn) { newValue in
            if newValue {
                startBlinking()
            } else {
                stopBlinking()
            }
        }
    }
    
    private func sendCommand(command: String, httpMethod: String, params: [String: Any]) {
        print("sendCommand->command: \(command); httpMethod: \(httpMethod)")
        guard let url = URL(string: "http://\(myArduinoIPAddress)/\(command)") else {
            print("Invalid URL")
            return
        }
        var request = URLRequest(url: url)
        request.httpMethod = httpMethod
        if (httpMethod == "POST") {
            request.httpBody = try? JSONSerialization.data(withJSONObject: params)
        }
        let task = URLSession.shared.dataTask(with: request) { data, response, error in
            if let error = error {
                print("Error occurred: \(error)")
                return
            }
            if let httpResponse = response as? HTTPURLResponse {
                print("HTTP Status Code: \(httpResponse.statusCode)")
                print("HTTP Method: \(request.httpMethod ?? "Unknown")")
                if let mimeType = httpResponse.mimeType {
                    print("MIME Type: \(mimeType)")
                }
            }
            if let data = data, let responseString = String(data: data, encoding: .utf8) {
                print("Response Data: \(responseString)")
                handleResponse(responseString)
            } else {
                print("No response data received or unable to decode data.")
            }
        }
        task.resume()
    }
    
    private func handleResponse(_ responseString: String) {
        // Parse the JSON response
        if let data = responseString.data(using: .utf8),
           let jsonResponse = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
           let status = jsonResponse["status"] as? String {
            let year = String(status.prefix(4))
            // let bitfieldValue = status.dropFirst(21) // Get the bitfield value as a substring

            var startIndex = status.index(status.startIndex, offsetBy: 21)
            var endIndex = status.index(startIndex, offsetBy: 3)
            let onoffBitfieldValue = status[startIndex..<endIndex]
            
            startIndex = status.index(status.startIndex, offsetBy: 26)
            endIndex = status.index(startIndex, offsetBy: 3)
            let arduinoStatusWord = status[startIndex..<endIndex]
            
            print("onoffBitfieldValue: \(onoffBitfieldValue)")
            print("arduinoStatusWord: \(arduinoStatusWord)")
            
            if let uint16Value = UInt16(onoffBitfieldValue) {
                print("uint16Value \(uint16Value)")
                var onoffstatus: Int = Int(uint16Value & 0x80)
                let bitfield = Int(uint16Value & 0x7f)
                print("onoffstatus: \(onoffstatus)")
                print("bitfield: \(bitfield)")
                onoffstatus = onoffstatus>>7 & 0x01
                print("onoffstatus: \(onoffstatus)")
                DispatchQueue.main.async {
                    activeDaysBitfield = bitfield
                }
                // Update the Sprinkler ON LED based on the last character
                DispatchQueue.main.async {
                    //isSprinklerOn = (lastCharacter == "1")
                    isSprinklerOn = (onoffstatus == 1)
                }
            } else {
                print("Conversion failed. The string does not represent a valid UInt16 value.")
            }
            
            if let uint8Value = UInt8(arduinoStatusWord) {
                print("uint8Value \(uint8Value)")
                let isScheduleInvalid: Int = Int(uint8Value & 0x01)
                let isHeapMemLow: Int = Int(uint8Value>>1 & 0x01)
                let isTimeStampNotSet: Int = Int(uint8Value>>2 & 0x01)
                let arduinoCommandError: Int = Int(uint8Value>>3 & 0x01)
                let isRSSIWeak: Int = Int(uint8Value>>4 & 0x01)
                
                print("isScheduleInvalid: \(isScheduleInvalid)")
                print("isHeapMemLow: \(isHeapMemLow)")
                print("isTimeStampNotSet: \(isTimeStampNotSet)")
                print("arduinoCommandError: \(arduinoCommandError)")
                print("isRSSIWeak: \(isRSSIWeak)")
                
                badSchedule = (isScheduleInvalid == 1) ? true : false
                badHeapMem_ = (isHeapMemLow == 1) ? true : false
                badTimeStmp = (isTimeStampNotSet == 1) ? true : false
                badRSSIsign = (arduinoCommandError == 1) ? true : false
                badCommand_ = (isRSSIWeak == 1) ? true : false
                
            } else {
                print("Conversion failed. The string does not represent a valid UInt8 value.")
            }
            
            
            if year == "1970" {
                // If the year is "1970", call the functions to send the current date and time
                sendDateTimeToArduino { response in
                    DispatchQueue.main.async {
                        responseMessage = response
                    }
                }
            } else {
                DispatchQueue.main.async {
                    responseMessage = "Received valid response: \(status)"
                }
            }
        } else {
            DispatchQueue.main.async {
                responseMessage = "Invalid JSON response"
            }
        }
    }
    
    /* private func sendSetScheduleCommand() {
        guard let url = URL(string: "http://\(myArduinoIPAddress)/SCH") else { return }
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let scheduleArray = schedule.map { ["dayOfWeek": $0.dayOfWeek, "time": $0.time.timeIntervalSince1970] }
        let parameters: [String: Any] = [
            "numberOfZones": numberOfZones,
            "duration": duration,
            "schedule": scheduleArray
        ]
        request.httpBody = try? JSONSerialization.data(withJSONObject: parameters)
        URLSession.shared.dataTask(with: request).resume()
    } */
    
    private func sendSetScheduleCommand() {
        guard let url = URL(string: "http://\(myArduinoIPAddress)/SCH") else { return }
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        
        // Convert the time to "HH:mm" format for each schedule entry
        let timeFormatter = DateFormatter()
        timeFormatter.dateFormat = "HH:mm"
        
        let scheduleArray = schedule.map {
            ["dayOfWeek": $0.dayOfWeek, "time": timeFormatter.string(from: $0.time)]
        }
        
        let parameters: [String: Any] = [
            "numberOfZones": numberOfZones,
            "duration": duration,
            "schedule": scheduleArray
        ]
        
        request.httpBody = try? JSONSerialization.data(withJSONObject: parameters)
        URLSession.shared.dataTask(with: request).resume()
    }
    
    private func getDate() -> Date {
        var dateComponents = DateComponents()
        dateComponents.year = year
        dateComponents.month = month
        dateComponents.day = day
        dateComponents.hour = hour
        dateComponents.minute = minute
        dateComponents.second = second
        return Calendar.current.date(from: dateComponents) ?? Date()
    }
    
    private func setDate(date: Date) {
        let components = Calendar.current.dateComponents([.year, .month, .day, .hour, .minute, .second], from: date)
        year = components.year ?? 2024
        month = components.month ?? 1
        day = components.day ?? 1
        hour = components.hour ?? 0
        minute = components.minute ?? 0
        second = components.second ?? 0
    }
    
    private func dayName(for index: Int) -> String {
        let dateFormatter = DateFormatter()
        return dateFormatter.weekdaySymbols[index]
    }
    
    private func getCurrentDateTimeISO8601() -> String {
        let formatter = ISO8601DateFormatter()
        formatter.timeZone = TimeZone(identifier: "America/New_York")
        formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        let dateTimeString = formatter.string(from: Date())
        return dateTimeString
    }
    
    func performActionWithDelay(delay: Double, action: @escaping () -> Void) {
        DispatchQueue.main.asyncAfter(deadline: .now() + delay) {
            action()  // Execute the passed function after the delay
        }
    }
    
    private func sendDateTimeToArduino(completion: @escaping (String) -> Void) {
        let dateTimeString = getCurrentDateTimeISO8601()
        let json: [String: Any] = ["datetime": dateTimeString]
        
        guard let jsonData = try? JSONSerialization.data(withJSONObject: json) else {
            completion("Error: Unable to serialize JSON")
            return
        }
        
        let urlString = "http://\(myArduinoIPAddress)/TIM"
        var request = URLRequest(url: URL(string: urlString)!)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = jsonData
        
        let task = URLSession.shared.dataTask(with: request) { data, response, error in
            guard let data = data, error == nil else {
                completion("Error: \(error?.localizedDescription ?? "Unknown error")")
                return
            }
            
            if let httpStatus = response as? HTTPURLResponse, httpStatus.statusCode != 200 {
                completion("HTTP Status Code: \(httpStatus.statusCode)")
                return
            }
            
            let responseString = String(data: data, encoding: .utf8) ?? "No response data"
            completion(responseString)
        }
        
        task.resume()
    }
    
    private func startBlinking() {
        blinkOpacity = 0.0
        withAnimation {
            blinkOpacity = 0.0
        }
    }
    
    private func stopBlinking() {
        withAnimation {
            blinkOpacity = 1.0
        }
    }
    
    private func startTimer() {
            // Create a repeating timer that triggers every HEARTBEAT_INTERVAL seconds
            timer = Timer.scheduledTimer(withTimeInterval: HEARTBEAT_INTERVAL, repeats: true) { _ in
                sendCommand(command: "HI!", httpMethod: "GET", params: [:])
            }
    }

    private func stopTimer() {
            // Invalidate the timer when the view disappears to avoid memory leaks
            timer?.invalidate()
            timer = nil
    }
    
    private func dayShortName(for index: Int) -> String {
            // Return short names for the days of the week
            let shortNames = ["Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"]
            return shortNames[index]
        }
}

struct SprinklerSchedule: Identifiable {
    var id = UUID()
    var dayOfWeek: Int
    var time: Date
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
