#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <stack>
#include <locale>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#endif

using namespace std;

struct Position {
    int line;
    int col;
};

struct ErrorInfo {
    string type;
    string message;
    Position pos;
    string clearInstruction; // Дополнительное простое пояснение для пользователя
};

// Функция перевода строки в нижний регистр для регистронезависимого сравнения
string myToLower(string s) {
    for (size_t i = 0; i < s.length(); i++) {
        if (s[i] >= 'A' && s[i] <= 'Z') {
            s[i] = s[i] + 32;
        }
    }
    return s;
}

// Проверка, является ли тег одиночным (самозакрывающимся)
bool isSelfClosing(string tag) {
    tag = myToLower(tag);
    return (tag == "br" || tag == "hr" || tag == "img" || tag == "input" || tag == "link" || tag == "meta");
}

bool isValidTagName(const string& name) {
    if (name.empty()) return false;
    unsigned char first = name[0];
    bool isEnglishLetter = (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z');//флаг для проверки начала тега(буква английская-true, иначе - false)
    if (!isEnglishLetter) return false;

    for (size_t i = 1; i < name.length(); i++) {
        unsigned char c = name[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            (c == '-') || (c == '_') || (c == ':') || (c == '.');//флаг для проверки валидности остальных символов тега(true - усли валидны, иначе - false)
        if (!ok) return false;
    }
    return true;
}

bool isValidAttributeName(const string& name) {
    if (name.empty()) return false;
    unsigned char first = name[0];
    bool isEnglishLetter = (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z');//флаг для проверки начала тега(буква английская-true, иначе - false)
    bool isSpecialAllowed = (first == '_') || (first == ':');//проверка атрибута начинается ли он с двоеточия или с нижнего подчеркивания(true - если да, иначе - false)
    if (!isEnglishLetter && !isSpecialAllowed) return false;

    for (size_t i = 1; i < name.length(); i++) {
        unsigned char c = name[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            (c == '-') || (c == '_') || (c == ':') || (c == '.');//флаг для проверки валидности остальных символов тега(true - усли валидны, иначе - false)
        if (!ok) return false;
    }
    return true;
}

vector<ErrorInfo> allErrors;
vector<pair<string, Position>> tagsStack;
bool hasTagsFound = false;

// Основной разбор структуры HTML и выявление ошибок с рекомендациями по исправлению
void startAnalysis(const string& text) {
    allErrors.clear();
    tagsStack.clear();
    hasTagsFound = false;

    int n = text.length();
    vector<Position> charPos(n + 1);
    int currentLine = 1;
    int currentCol = 1;
    for (int idx = 0; idx <= n; idx++) {
        charPos[idx] = { currentLine, currentCol };//считываем текст и обеспечиваем нормальное чтение текста из файла
        if (idx < n) {
            if (text[idx] == '\n') {
                currentLine++;
                currentCol = 1;
            }
            else {
                unsigned char c = text[idx];
                if ((c & 0xC0) != 0x80) { 
                    currentCol++;
                }
            }
        }
    }

    for (int i = 0; i < n; i++) {
        // Пропускаем комментарии <!-- ... -->образом
        if (i + 3 < n && text[i] == '<' && text[i + 1] == '!' && text[i + 2] == '-' && text[i + 3] == '-') {
            size_t found = text.find("-->", i + 4);
            if (found != string::npos) {
                // Продвигаем курсоры строк и столбцов по тесту при пропуске комментария
                int old_i = i;
                i = found + 2; 
                continue;
            }
            else {
                i = n;
                break;
            }
        }
        // Пропускаем декларации <!DOCTYPE ... > или аналогичные правильным образом
        if (i + 1 < n && text[i] == '<' && text[i + 1] == '!') {
            int j = i + 2;
            bool inQuotes = false;//true, если внутри кавычек (игнорируем символ '>'), false — снаружи флаг для проверки комментариев(чтобы код не ломался)
            while (j < n && (inQuotes || text[j] != '>')) {
                if (text[j] == '"' || text[j] == '\'') {
                    inQuotes = !inQuotes;
                }
                j++;
            }
            i = j; 
            continue;
        }

        if (text[i] == '<') {
            hasTagsFound = true;  //флаг для нахождения хотябы одного тега в файле(true - если тег в файле есть, false - если нет)
            Position startPos = charPos[i];
            int j = i + 1;
            if (j >= n) break;

            bool isClosing = (text[j] == '/');//определяет тип тега(true - закрывающий или false - открывающий)
            if (isClosing) j++;

            string tagName = "";
            while (j < n && !isspace((unsigned char)text[j]) && text[j] != '>' && text[j] != '/' && text[j] != '=' && text[j] != '"' && text[j] != '\'') {
                tagName += text[j];
                j++;
            }

            // Проверяем имя тега на корректность
            if (tagName != "") {
                if (!isValidTagName(tagName)) {
                    string fullTagName = isClosing ? ("/" + tagName) : tagName;
                    string msg = "Недопустимое имя тега <" + fullTagName + ">";
                    string instr = "Пожалуйста, исправьте имя тега <" + fullTagName + ">.";
                    allErrors.push_back({ "Синтаксис", msg, startPos, instr });
                }
            }

            // Пропускаем атрибуты тега до закрывающей угловой скобки '>' и валидируем их названия
            int attrStart = j;
            bool inQuotes = false;//защита от ложного срабатывания(если > встретится в самом тексте и программа не поссчитает его тэгом)
            while (j < n && (inQuotes || text[j] != '>')) {
                if (text[j] == '"' || text[j] == '\'') {
                    inQuotes = !inQuotes;
                }
                j++;
            }

            if (j >= n) {
                string msg = "Тег не закрыт символом '>'";
                string instr = "Вы написали '<' у тега на Строке " + to_string(startPos.line) + ", но забыли поставить закрывающую скобку '>' в конце тега. Поставьте '>'.";
                allErrors.push_back({ "Синтаксис", msg, startPos, instr });
                break;
            }

            // Парсим и проверяем названия атрибутов
            string tagInnerAttrs = text.substr(attrStart, j - attrStart);
            size_t idx = 0;
            while (idx < tagInnerAttrs.length()) {
                while (idx < tagInnerAttrs.length() && isspace((unsigned char)tagInnerAttrs[idx])) {
                    idx++;
                }
                if (idx >= tagInnerAttrs.length()) break;
                if (tagInnerAttrs[idx] == '/') {
                    idx++;
                    continue;
                }

                size_t nameStart = idx;
                while (idx < tagInnerAttrs.length() && !isspace((unsigned char)tagInnerAttrs[idx]) && tagInnerAttrs[idx] != '=' && tagInnerAttrs[idx] != '/' && tagInnerAttrs[idx] != '"' && tagInnerAttrs[idx] != '\'') {
                    idx++;
                }
                string attrName = tagInnerAttrs.substr(nameStart, idx - nameStart);

                if (!attrName.empty()) {
                    if (!isValidAttributeName(attrName)) {
                        Position errPos = charPos[attrStart + nameStart];
                        // Если позиция за границей, берем startPos
                        if (attrStart + nameStart >= charPos.size()) {
                            errPos = startPos;
                        }
                        string msg = "Недопустимое имя атрибута \"" + attrName + "\" в теге <" + tagName + ">";
                        string instr = "Имя атрибута должно начинаться с буквы и содержать только буквы, цифры, дефисы, двоеточия или подчеркивания. Исправьте имя \"" + attrName + "\".";
                        allErrors.push_back({ "Синтаксис", msg, errPos, instr });
                    }
                }

                // Пропускаем пробелы до '='
                while (idx < tagInnerAttrs.length() && isspace((unsigned char)tagInnerAttrs[idx])) {
                    idx++;
                }

                if (idx < tagInnerAttrs.length() && tagInnerAttrs[idx] == '=') {
                    idx++;
                    while (idx < tagInnerAttrs.length() && isspace((unsigned char)tagInnerAttrs[idx])) {
                        idx++;
                    }
                    if (idx < tagInnerAttrs.length()) {
                        if (tagInnerAttrs[idx] == '"' || tagInnerAttrs[idx] == '\'') {
                            char quote = tagInnerAttrs[idx];
                            idx++;
                            while (idx < tagInnerAttrs.length() && tagInnerAttrs[idx] != quote) {
                                idx++;
                            }
                            if (idx < tagInnerAttrs.length()) {
                                idx++;
                            }
                        }
                        else {
                            while (idx < tagInnerAttrs.length() && !isspace((unsigned char)tagInnerAttrs[idx]) && tagInnerAttrs[idx] != '/') {
                                idx++;
                            }
                        }
                    }
                }
            }

            if (tagName != "") {
                string lowName = myToLower(tagName);
                if (isClosing) {
                    if (tagsStack.empty()) {
                        string msg = "Тег </" + tagName + "> не был открыт";
                        string instr = "Вы закрываете тег </" + tagName + "> на Строке " + to_string(startPos.line) + ", но тег <" + tagName + "> никогда не открывался ранее. Пожалуйста, удалите этот закрывающий тег </" + tagName + "> или откройте его перед этим местом.";
                        allErrors.push_back({ "Структура", msg, startPos, instr });
                    }
                    else {
                        int foundIdx = -1;
                        for (int k = (int)tagsStack.size() - 1; k >= 0; k--) {
                            if (tagsStack[k].first == lowName) {
                                foundIdx = k;
                                break;
                            }
                        }

                        if (foundIdx != -1) {
                            if (tagsStack.back().first != lowName) {
                                string msg = "Нарушена последовательность тегов (ожидался закрывающий </" + tagsStack.back().first + ">)";
                                string instr = "Вы закрываете </" + tagName + "> на Строке " + to_string(startPos.line) + " (Столбец " + to_string(startPos.col) +
                                    "), но последним был открыт тег <" + tagsStack.back().first + "> на Строке " + to_string(tagsStack.back().second.line) +
                                    " (Столбец " + to_string(tagsStack.back().second.col) + "). Пожалуйста, сначала закройте </" + tagsStack.back().first + ">.";
                                allErrors.push_back({ "Структура", msg, startPos, instr });
                            }
                            // Удаляем все элементы от вершины стека до найденного тега включительно
                            tagsStack.erase(tagsStack.begin() + foundIdx, tagsStack.end());
                        }
                        else {
                            string msg = "Тег </" + tagName + "> не был открыт";
                            string instr = "Вы закрываете тег </" + tagName + "> на Строке " + to_string(startPos.line) + ", но тег <" + tagName + "> никогда не открывался ранее. Пожалуйста, удалите этот закрывающий тег </" + tagName + "> или откройте его перед этим местом.";
                            allErrors.push_back({ "Структура", msg, startPos, instr });
                        }
                    }
                }
                else {
                    // Проверяем, является ли тег самозакрывающимся (по списку или по знаку '/' перед '>')
                    bool checkSelfClosing = isSelfClosing(lowName);
                    if (!checkSelfClosing) {
                        int k = j - 1;
                        while (k > i && isspace((unsigned char)text[k])) {
                            k--;
                        }
                        if (k > i && text[k] == '/') {
                            checkSelfClosing = true;
                        }
                    }

                    if (!checkSelfClosing) {
                        // Проверка на повторное открытие тегов без закрытия
                        int foundIdx = -1;
                        for (int k = (int)tagsStack.size() - 1; k >= 0; k--) {
                            if (tagsStack[k].first == lowName) {
                                foundIdx = k;
                                break;
                            }
                        }

                        if (foundIdx != -1 && (lowName == "p" || lowName == "li" || lowName == "html" || lowName == "body" ||
                            lowName == "head" || lowName == "title" || lowName == "h1" || lowName == "h2" ||
                            lowName == "h3" || lowName == "h4" || lowName == "h5" || lowName == "h6")) {
                            Position prevPos = tagsStack[foundIdx].second;
                            string msg = "Тег <" + tagName + "> открыт повторно без закрытия предыдущего <" + tagName + ">";
                            string instr = "Вы открыли новый тег <" + tagName + "> на Строке " + to_string(startPos.line) + ", Столбце " + to_string(startPos.col) +
                                ", однако предыдущий тег <" + tagName + "> (открытый выше на Строке " + to_string(prevPos.line) + ", Столбец " + to_string(prevPos.col) +
                                ") все еще не закрыт. Пожалуйста, закройте первый тег с помощью </" + tagName + "> перед тем, как открывать новый.";
                            allErrors.push_back({ "Структура", msg, startPos, instr });
                            tagsStack.erase(tagsStack.begin() + foundIdx);
                        }

                        tagsStack.push_back({ lowName, startPos });
                    }
                }
            }

            // Обновляем текущую позицию курсора до закрывающей скобки
            i = j;
        }
    }

    // Теги, которые остались открытыми в стеке по завершении файла
    while (!tagsStack.empty()) {
        string tagName = tagsStack.back().first;
        Position openPos = tagsStack.back().second;
        string msg = "Тег <" + tagName + "> остался открытым и не был закрыт";
        string instr = "Вы открыли тег <" + tagName + "> на Строке " + to_string(openPos.line) + ", Столбце " + to_string(openPos.col) + ", но забыли закрыть его с помощью </" + tagName + "> далее в коде. Пожалуйста, добавьте </" + tagName + "> там, где этот элемент должен заканчиваться.";
        allErrors.push_back({ "Структура", msg, openPos, instr });
        tagsStack.pop_back();
    }
}

// Функция чистого извлечения текста без тегов
string extractPlainText(const string& text) {
    string output = "";
    bool inTag = false;//фильтр html тегов(true - когда встречает < , а когда > становится false) программа благодаря ему понимает какой текст в файл не записывать
    string tagBuf = "";

    for (size_t i = 0; i < text.length(); i++) {
        if (text[i] == '<') {
            inTag = true;
            tagBuf = "";
        }
        else if (text[i] == '>') {
            inTag = false;
            string t = myToLower(tagBuf);
            if (t == "p" || t == "/p" || t == "div" || t == "br" || t == "h1") {
                output += "\n";
            }
        }
        else {
            if (!inTag) {
                output += text[i];
            }
            else {
                tagBuf += text[i];
            }
        }
    }
    return output;
}

// Инициализация консоли и локализации для поддержки русского языка
void initLocale() {
#ifdef _WIN32
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
#endif
    setlocale(LC_ALL, "Russian");
}

// Вывод приветственного меню и заголовка программы
void printMenuHeader() {
    cout << "\n====================================================" << endl;
    cout << "              ПРОГРАММА: АНАЛИЗАТОР HTML               " << endl;
    cout << "====================================================" << endl;
    cout << "Введите имя файла для анализа без расширения (или 'exit' для выхода):" << endl;
    cout << "-> ";
}

// Функция для определения реального имени файла с учетом расширения
string resolveFileName(const string& fileNameInput, bool& hasInvalidExtension, size_t& dotPos) {
    dotPos = fileNameInput.find_last_of('.');
    string resolvedName = "";
    hasInvalidExtension = false;// флаг для проверки расширения true - если расшерение .html или .htm, иначе - false

    // Проверяем формат файла (если пользователь ввел расширение)
    if (dotPos != string::npos) {
        string ext = myToLower(fileNameInput.substr(dotPos));
        if (ext == ".html" || ext == ".htm") {
            resolvedName = fileNameInput;
        }
        else {
            hasInvalidExtension = true;
        }
    }
    else {
        // Пытаемся автоматически найти .html или .htm
        string htmlPath = fileNameInput + ".html";
        string htmPath = fileNameInput + ".htm";

        ifstream testHtml(htmlPath.c_str());
        if (testHtml.good()) {
            resolvedName = htmlPath;
            testHtml.close();
        }
        else {
            ifstream testHtm(htmPath.c_str());
            if (testHtm.good()) {
                resolvedName = htmPath;
                testHtm.close();
            }
            else {
                // Если ни одного нет, по умолчанию пробуем открыть .html для вызова File Not Found
                resolvedName = htmlPath;
            }
        }
    }
    return resolvedName;
}

// Системное уведомление 2: Неверный формат файла
void printInvalidExtensionWarning(const string& fileNameInput) {
    cout << "\n====================================================" << endl;
    cout << "[СИСТЕМНОЕ УВЕДОМЛЕНИЕ: НЕВЕРНЫЙ ФОРМАТ ФАЙЛА]" << endl;
    cout << "Ошибка: Файл должен иметь формат HTML или HTM!" << endl;
    cout << "Вы ввели: \"" << fileNameInput << "\"" << endl;
    cout << "Напоминание: вводите только имя файла БЕЗ расширения!" << endl;
    cout << "====================================================" << endl;
}

// Системное уведомление 1: Файл в принципе не найден
void printFileNotFoundWarning(const string& fileNameInput, const string& resolvedName, size_t dotPos) {
    cout << "\n====================================================" << endl;
    cout << "[СИСТЕМНОЕ УВЕДОМЛЕНИЕ: ФАЙЛ НЕ НАЙДЕН]" << endl;
    cout << "Ошибка: Файл \"" << (dotPos == string::npos ? (fileNameInput + ".html / .htm") : resolvedName) << "\" не обнаружен в каталоге программы." << endl;
    cout << "Пожалуйста, проверьте имя файла и убедитесь, что он существует." << endl;
    cout << "====================================================" << endl;
}

// Функция для считывания содержимого файла
bool readFileContent(const string& resolvedName, string& content) {
    ifstream file(resolvedName.c_str());
    if (!file.is_open()) {
        return false;
    }

    content = "";
    string line;
    while (getline(file, line)) {
        content += line + "\n";
    }
    file.close();
    return true;
}

// Вывод ошибок в КОНСОЛЬ
void printAnalysisResults() {
    if (!hasTagsFound) {
        cout << "\n>>> ИТОГ: В файле не обнаружено ни одного HTML-тега. <<<" << endl;
        return;
    }
    if (allErrors.empty()) {
        cout << "\n>>> ИТОГ: Ошибок в разметке не обнаружено! <<<" << endl;
    }
    else {
        cout << "\n>>> ИТОГ АНАЛИЗА: ОБНАРУЖЕНО ОШИБОК (" << allErrors.size() << ") <<<" << endl;
        cout << "----------------------------------------------------" << endl;
        for (size_t i = 0; i < allErrors.size(); i++) {
            cout << i + 1 << ". [" << allErrors[i].type << " ОШИБКА]" << endl;
            cout << "   " << allErrors[i].message << endl;
            cout << "   Позиция -> Строка: " << allErrors[i].pos.line << ", Столбец: " << allErrors[i].pos.col << endl;
            cout << "   Рекомендация по исправлению: " << allErrors[i].clearInstruction << endl;
            cout << "----------------------------------------------------" << endl;
        }
    }
}

// Проверка перед созданием/перезаписью файлов отчета
bool checkOverwriteFile() {
    ifstream checkOut("output.txt");
    ifstream checkErr("errors.txt");
    if (checkOut.good() || checkErr.good()) {
        if (checkOut.good()) checkOut.close();
        if (checkErr.good()) checkErr.close();
        cout << "\n[!] Предупреждение: Файлы результатов (\"output.txt\" или \"errors.txt\") уже существуют." << endl;
        cout << "Вы уверены, что хотите перезаписать данные? (y/n): " << flush;

        char ch = 0;
        while (true) {
#ifdef _WIN32
            ch = _getch();
#else
            ch = cin.get();
#endif
            // Приводим символ к нижнему регистру для английского 
            if (ch >= 'A' && ch <= 'Z') {
                ch = ch + 32;
            }
            // Приводим к нижнему регистру для русского CP1251 
            if (ch == (char)196) { // 'Д'
                ch = (char)228;    // 'д'
            }
            if (ch == (char)205) { // 'Н'
                ch = (char)237;    // 'н'
            }

            if (ch == 'y' || ch == 'n' || ch == (char)228 || ch == (char)237) {
                break;
            }
        }

        // Выводим выбранный символ на экран
        if (ch == (char)228) {
            cout << "y" << endl;
        }
        else if (ch == (char)237) {
            cout << "n" << endl;
        }
        else {
            cout << ch << endl;
        }

        if (ch != 'y' && ch != (char)228) {
            cout << "[!] Сохранение отменено. Старые файлы сохранены." << endl;
            return false;
        }
    }
    return true;
}

// Сохранение результатов анализа во внешние файлы-отчеты
bool writeReportFiles(const string& resolvedName, const string& content) {
    // Запись в первый файл: чистый извлеченный текст (output.txt)
    ofstream out("output.txt");
    if (!out.is_open()) {
        cout << "[!] Ошибка: Не удалось записать результаты в output.txt." << endl;
        return false;
    }

    out << extractPlainText(content);
    out.close();

    // Запись во второй файл: список ошибок (errors.txt) — создается только если есть ошибки
    if (!allErrors.empty()) {
        ofstream errFile("errors.txt");
        if (!errFile.is_open()) {
            cout << "[!] Ошибка: Не удалось записать список ошибок в errors.txt." << endl;
            return false;
        }
        else {
            errFile << "--- СПИСОК ОБНАРУЖЕННЫХ ОШИБОК В ФАЙЛЕ: " << resolvedName << " ---\n";
            errFile << "Всего найдено несоответствий: " << allErrors.size() << "\n\n";
            for (size_t i = 0; i < allErrors.size(); i++) {
                errFile << i + 1 << ". ТИП: [" << allErrors[i].type << "]\n";
                errFile << "   ОПИСАНИЕ: " << allErrors[i].message << "\n";
                errFile << "   МЕСТО: Строка " << allErrors[i].pos.line << ", Столбец " << allErrors[i].pos.col << "\n";
                errFile << "   ИНСТРУКЦИЯ ПО ИСПРАВЛЕНИЮ: " << allErrors[i].clearInstruction << "\n\n";
            }
            errFile.close();
        }
    }
    return true;
}

int main() {
    initLocale();

    while (true) {
        printMenuHeader();
        string fileNameInput;
        getline(cin, fileNameInput);

        // Проверка на выход
        if (myToLower(fileNameInput) == "exit") {
            cout << "\nЗавершение работы программы. До свидания!" << endl;
            break;
        }

        if (fileNameInput.empty()) {
            cout << "[!] Введена пустая строка. Пожалуйста, укажите имя файла." << endl;
            continue;
        }

        bool hasInvalidExtension = false;//проверка ввода пользователем(true - когда пользователь ввел имя файла с расширением но расширение не html или htm)
        size_t dotPos = string::npos;
        string resolvedName = resolveFileName(fileNameInput, hasInvalidExtension, dotPos);

        // Системное уведомление 2: Неверный формат файла
        if (hasInvalidExtension) {
            printInvalidExtensionWarning(fileNameInput);
            continue;
        }

        string content = "";
        // Системное уведомление 1: Файл в принципе не найден
        if (!readFileContent(resolvedName, content)) {
            printFileNotFoundWarning(fileNameInput, resolvedName, dotPos);
            continue;
        }

        cout << "\n[ Анализ файла \"" << resolvedName << "\"... ]" << endl;
        startAnalysis(content);

        // Вывод ошибок в КОНСОЛЬ
        printAnalysisResults();

        if (!hasTagsFound) {
            cout << "\n====================================================" << endl;
            cout << "[СИСТЕМНОЕ УВЕДОМЛЕНИЕ: HTML-ТЕГИ НЕ ОБНАРУЖЕНЫ]" << endl;
            cout << "Внимание: В выбранном файле не найдено ни одного HTML-тега." << endl;
            cout << "Отчеты не сформированы, существующие файлы не изменены." << endl;
            cout << "====================================================" << endl;
            continue;
        }

        // Проверка перед созданием/перезаписью файлов отчета
        if (!checkOverwriteFile()) {
            continue;
        }

        // Запись результатов во внешние файлы
        if (writeReportFiles(resolvedName, content)) {
            cout << "[OK] Анализ завершен!" << endl;
            cout << "[OK] Отчет сохранен в файл \"output.txt\"." << endl;
            if (!allErrors.empty()) {
                cout << "[OK] Простой список ошибок сохранен в файл \"errors.txt\"." << endl;
            }
        }
    }
    return 0;
}
