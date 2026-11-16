#ifndef CIBORGUE_MEMORY_MANAGER_H
#define CIBORGUE_MEMORY_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <vector>

struct TodoTaskItem {
    int id;
    String title;
    bool done;
};

class MemoryManager {
private:
    Preferences prefs;
    std::vector<String> memories;
    std::vector<TodoTaskItem> tasks;
    int nextTaskId = 1;

    void saveMemoriesToNvs() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (const auto& m : memories) {
            arr.add(m);
        }
        String out;
        serializeJson(doc, out);
        prefs.begin("cib_mem", false);
        prefs.putString("memories", out);
        prefs.end();
        emitMemoriesPacket();
    }

    void saveTasksToNvs() {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (const auto& t : tasks) {
            JsonObject obj = arr.add<JsonObject>();
            obj["id"] = t.id;
            obj["title"] = t.title;
            obj["done"] = t.done;
        }
        String out;
        serializeJson(doc, out);
        prefs.begin("cib_mem", false);
        prefs.putString("tasks", out);
        prefs.putInt("next_id", nextTaskId);
        prefs.end();
        emitTasksPacket();
    }

public:
    void begin() {
        prefs.begin("cib_mem", true);
        String memStr = prefs.getString("memories", "[]");
        String taskStr = prefs.getString("tasks", "[]");
        nextTaskId = prefs.getInt("next_id", 1);
        prefs.end();

        memories.clear();
        JsonDocument mDoc;
        if (!deserializeJson(mDoc, memStr) && mDoc.is<JsonArray>()) {
            for (JsonVariant v : mDoc.as<JsonArray>()) {
                if (v.is<const char*>()) {
                    memories.push_back(v.as<String>());
                }
            }
        }

        tasks.clear();
        JsonDocument tDoc;
        if (!deserializeJson(tDoc, taskStr) && tDoc.is<JsonArray>()) {
            for (JsonObject o : tDoc.as<JsonArray>()) {
                TodoTaskItem item;
                item.id = o["id"] | nextTaskId++;
                item.title = o["title"] | "";
                item.done = o["done"] | false;
                if (item.title.length() > 0) {
                    tasks.push_back(item);
                    if (item.id >= nextTaskId) nextTaskId = item.id + 1;
                }
            }
        }
    }

    // ========================================================================
    // 1. MEMÓRIAS DE LONGO PRAZO (Fatos, Preferências e Notas do Usuário)
    // ========================================================================
    String addMemory(String fact) {
        fact.trim();
        if (fact.length() == 0) return "Fato vazio.";
        for (const auto& existing : memories) {
            if (existing.equalsIgnoreCase(fact)) {
                return "Eu já tinha essa informação guardada na minha memória Flash: " + fact;
            }
        }
        if (memories.size() >= 15) {
            memories.erase(memories.begin());
        }
        memories.push_back(fact);
        saveMemoriesToNvs();
        return "Memória salva permanentemente na Flash NVS do ESP32: " + fact;
    }

    String removeMemory(int index1Based) {
        int idx = index1Based - 1;
        if (idx >= 0 && idx < (int)memories.size()) {
            String removed = memories[idx];
            memories.erase(memories.begin() + idx);
            saveMemoriesToNvs();
            return "Memória removida: " + removed;
        }
        return "Índice de memória inválido.";
    }

    String clearMemories() {
        memories.clear();
        saveMemoriesToNvs();
        return "Todas as memórias de longo prazo na Flash NVS foram apagadas.";
    }

    // ========================================================================
    // 2. LISTA DE TAREFAS / TO-DO PERSISTENTE NA FLASH NVS
    // ========================================================================
    String addTask(String title) {
        title.trim();
        if (title.length() == 0) return "Título da tarefa vazio.";
        if (tasks.size() >= 20) {
            // Remove a mais antiga concluída se estiver cheio
            for (auto it = tasks.begin(); it != tasks.end(); ++it) {
                if (it->done) {
                    tasks.erase(it);
                    break;
                }
            }
        }
        TodoTaskItem t;
        t.id = nextTaskId++;
        t.title = title;
        t.done = false;
        tasks.push_back(t);
        saveTasksToNvs();
        return String("Tarefa #") + String(t.id) + " adicionada na Flash NVS: " + title;
    }

    String completeTask(int idOrIndex, const String& titleHint = "") {
        for (auto& t : tasks) {
            if (t.id == idOrIndex || (titleHint.length() > 2 && t.title.indexOf(titleHint) != -1)) {
                t.done = true;
                saveTasksToNvs();
                return String("Tarefa concluída com sucesso: ") + t.title;
            }
        }
        int idx = idOrIndex - 1;
        if (idx >= 0 && idx < (int)tasks.size()) {
            tasks[idx].done = true;
            saveTasksToNvs();
            return String("Tarefa concluída: ") + tasks[idx].title;
        }
        return "Não encontrei essa tarefa na lista.";
    }

    String removeTask(int idOrIndex) {
        for (auto it = tasks.begin(); it != tasks.end(); ++it) {
            if (it->id == idOrIndex) {
                String name = it->title;
                tasks.erase(it);
                saveTasksToNvs();
                return "Tarefa removida: " + name;
            }
        }
        int idx = idOrIndex - 1;
        if (idx >= 0 && idx < (int)tasks.size()) {
            String name = tasks[idx].title;
            tasks.erase(tasks.begin() + idx);
            saveTasksToNvs();
            return "Tarefa removida: " + name;
        }
        return "Tarefa não localizada para remoção.";
    }

    String clearTasks(bool onlyCompleted = false) {
        if (!onlyCompleted) {
            tasks.clear();
            saveTasksToNvs();
            return "Lista de tarefas limpa na Flash NVS.";
        }
        for (auto it = tasks.begin(); it != tasks.end();) {
            if (it->done) it = tasks.erase(it);
            else ++it;
        }
        saveTasksToNvs();
        return "Tarefas concluídas removidas.";
    }

    String getTasksSummaryText() const {
        if (tasks.empty()) return "Você não tem nenhuma tarefa cadastrada na lista no momento.";
        String pending = "";
        int pendCount = 0;
        int doneCount = 0;
        for (const auto& t : tasks) {
            if (!t.done) {
                pendCount++;
                if (pending.length() > 0) pending += "; ";
                pending += String("#") + String(t.id) + " " + t.title;
            } else {
                doneCount++;
            }
        }
        if (pendCount == 0) {
            return String("Todas as suas ") + String(doneCount) + " tarefas já foram concluídas! Parabéns!";
        }
        return String("Você tem ") + String(pendCount) + " tarefa(s) pendente(s): " + pending + ".";
    }

    // Bloco compacto injetado automaticamente no System Prompt do Gemini em toda conversa
    String buildContextForGemini() const {
        String ctx = "";
        if (!memories.empty()) {
            ctx += "\n[MEMÓRIAS SALVAS NA FLASH SOBRE O USUÁRIO]: ";
            for (size_t i = 0; i < memories.size(); i++) {
                if (i > 0) ctx += " | ";
                ctx += memories[i];
            }
        }
        if (!tasks.empty()) {
            ctx += "\n[LISTA DE TAREFAS ATUAL DO USUÁRIO]: ";
            for (size_t i = 0; i < tasks.size(); i++) {
                if (i > 0) ctx += " | ";
                ctx += String("#") + String(tasks[i].id) + " " + tasks[i].title + (tasks[i].done ? " (CONCLUÍDA)" : " (PENDENTE)");
            }
        }
        return ctx;
    }

    String getMemoriesJson() const {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (const auto& m : memories) arr.add(m);
        String out;
        serializeJson(doc, out);
        return out;
    }

    String getTasksJson() const {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (const auto& t : tasks) {
            JsonObject o = arr.add<JsonObject>();
            o["id"] = t.id;
            o["title"] = t.title;
            o["done"] = t.done;
        }
        String out;
        serializeJson(doc, out);
        return out;
    }

    void emitMemoriesPacket() const {
        Serial.printf("EVT:MEMORIES=%s\n", getMemoriesJson().c_str());
    }

    void emitTasksPacket() const {
        Serial.printf("EVT:TASKS=%s\n", getTasksJson().c_str());
    }

    void emitAllSyncPackets() const {
        emitMemoriesPacket();
        emitTasksPacket();
    }
};

#endif // CIBORGUE_MEMORY_MANAGER_H
