//
//  ContentView.swift
//  MyLawnSprinklerController
//
//  Created by William E. Laing, Jr. on 8/13/24.
//

import SwiftUI

struct ContentView: View {
    
    let myArduinoIPAddress = "192.168.0.238";
    
    @State private var month = 1
    @State private var day = 1
    @State private var year = 2024
    @State private var hour = 0
    @State private var minute = 0
    @State private var second = 0
    @State private var numberOfZones = 1
    @State private var duration = 0
    @State private var schedule: [SprinklerSchedule] = []
    //@State private var schedule: [SprinklerSchedule] = [SprinklerSchedule(dayOfWeek: 0, time: Date())]
    @State private var params:  [String: Any] = [:]

    
    var body: some View {
        VStack {
            Text("Hello, SwiftUI!")
            HStack {
                Button("ON") {
                    sendCommand(command: "ONN", httpMethod: "GET", params: [:])
                }
                .padding()
                Button("OFF") {
                    sendCommand(command: "OFF", httpMethod: "GET", params: [:])
                }
                .padding()
            }

            DatePicker("Set Timestamp", selection: Binding(
                get: { getDate() },
                set: { setDate(date: $0) }
            ), displayedComponents: [.date, .hourAndMinute])
                .padding()

            Button("SET_TIMESTAMP") {
                sendSetTimestampCommand()
            }
            .padding()

            Stepper("Number of Zones: \(numberOfZones)", value: $numberOfZones, in: 1...10)
                .padding()

            Stepper("Duration per Zone: \(duration) min", value: $duration, in: 0...60)
                .padding()

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
                                    DatePicker("Time", selection: Binding(
                                        get: { entry.time },
                                        set: { newValue in
                                            if let index = schedule.firstIndex(where: { $0.id == entry.id }) {
                                                schedule[index].time = newValue
                                            }
                                        }
                                    ), displayedComponents: [.hourAndMinute])
                                }
                            }
                            .onDelete { indexSet in
                                schedule.remove(atOffsets: indexSet)
                            }
                        }

                        Button("Add Schedule Entry") {
                            schedule.append(SprinklerSchedule(dayOfWeek: 0, time: Date()))
                        }
                        .padding()


            Button("SET_SCHEDULE") {
                sendSetScheduleCommand()
            }
            .padding()
        }
        .onAppear {
            // Send the "HI!" command when ContentView appears
            sendCommand(command: "HI!", httpMethod: "GET", params: [:])
        }
    }

    private func sendCommand(command: String, httpMethod: String, params: [String: Any] ) {
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
            } else {
                print("No response data received or unable to decode data.")
            }
        }

        task.resume()
        // URLSession.shared.dataTask(with: request).resume()
    }


    private func sendSetTimestampCommand() {
        guard let url = URL(string: "http://\(myArduinoIPAddress)/SET_TIMESTAMP") else { return }
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let parameters: [String: Any] = [
            "month": month,
            "day": day,
            "year": year,
            "hour": hour,
            "minute": minute,
            "second": second
        ]
        request.httpBody = try? JSONSerialization.data(withJSONObject: parameters)
        URLSession.shared.dataTask(with: request).resume()
    }

    private func sendSetScheduleCommand() {
        guard let url = URL(string: "http://\(myArduinoIPAddress)/SET_SCHEDULE") else { return }
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
}

//struct SprinklerSchedule {
    //var dayOfWeek: Int
    //var time: Date
//}

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



