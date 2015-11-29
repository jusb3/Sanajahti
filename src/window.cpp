#include <QtWidgets>
#include <chrono>
#include "window.hpp"
#include "adb_screenshot.hpp"

Window::Window()
{
    //init some text labels
    QLabel* label1=new QLabel("X size:", this);
    label1->move(5,5);

    QLabel* label2=new QLabel("Y size:", this);
    label2->move(85,5);

    QLabel* label4=new QLabel("Words:", this);
    label4->move(5,65);

    //init textpanel for x length
    xpanel= new QLineEdit("4",this);
    xpanel->move(40,3);
    xpanel->setFixedWidth(30);
    xpanel->setValidator(new QIntValidator(1, 99, this));
    connect(xpanel, SIGNAL(textChanged(const QString &)), this, SLOT(gridChange()));

    //init textpanel for y length
    ypanel= new QLineEdit("4",this);
    ypanel->move(120,3);
    ypanel->setFixedWidth(30);
    ypanel->setValidator(new QIntValidator(1, 99, this));
    connect(ypanel, SIGNAL(textChanged(const QString &)), this, SLOT(gridChange()));

    //init textpanel for library path
    library_path= new QLineEdit("",this);
    library_path->move(5,33);
    library_path->setFixedWidth(90);
    library_path->setReadOnly(true);
    library_path->setStyleSheet("background-color:lightgrey");

    //init button to select library
    browse_button= new QPushButton("Library",this);
    browse_button->move(100,31);
    browse_button->setFixedWidth(55);
    connect(browse_button,SIGNAL(clicked()),this,SLOT(browse()));

    //init startbutton
    start_button = new QPushButton("Start",this);
    start_button->move(5,91);
    start_button->setFixedWidth(45);
    connect(start_button,SIGNAL(clicked()),this,SLOT(manual_start()));

    restart_button = new QPushButton("Restart",this);
    restart_button->move(5,91);
    restart_button->setFixedWidth(45);
    restart_button->setHidden(true);
    connect(restart_button,SIGNAL(clicked()),this,SLOT(restart()));

    //init button for adb controlling
    adb_button = new QPushButton("Auto Solve", this);
    adb_button->move(60,91);
    connect(adb_button,SIGNAL(clicked()),this,SLOT(adb_start()));

    //init combobox for wordlist
    list = new QComboBox(this);
    list->move(50,62);
    list->setFixedWidth(105);
    list->setDisabled(true);
    connect(list, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(drawWord(const QString &)));

    //init mapper for pairing tile textChanged() signals to widget calling it
    mapper = new QSignalMapper(this);
    connect(mapper,SIGNAL(mapped(int)),this,SLOT(valueChange(int)));

    //call function to creat 4x4 grid
    makeGrid(4,4);
}

void Window::valueChange(int id)
{
    tiles.at(id)->setText(tiles.at(id)->text().toUpper());
    if(id<tiles.length()-1){
        tiles.at(++id)->setFocus();
        tiles.at(id)->selectAll();
    }
}

void Window::adb_start()
{
    xpanel->setText(QString::number(4));
    ypanel->setText(QString::number(4));
    gridChange();
    auto adbscr = ADBScreenshot();
    bool success = adbscr.TakeScreenshot("grid.png");
    if(!success){
        QMessageBox::information(this, tr("Error"), QString("There was a problem with connecting to the phone via ADB."));
        return;
    }
    OCR ocr("grid.png");
    for(int a=0;a<16;a++)
        tiles.at(a)->setText(QString::fromStdString(ocr.identifyLetter(a%4-1,a/4-1)));
}

void Window::manual_start()
{
    this->setWindowTitle("Sanajahti");
    list->clear();
    if(!gridFilled()){
        QMessageBox::information(this, tr("Error"), QString("You need to complete the grid!"));
        return;
    }
    if(library_path->text().isEmpty()||library_path->text().isNull()){
        QMessageBox::information(this, tr("Error"), QString("You need to specify the library to use!"));
        return;
    }
    for(auto obj:tiles)
        obj->setDisabled(true);
    start_button->setHidden(true);
    restart_button->setHidden(false);
    xpanel->setDisabled(true);
    ypanel->setDisabled(true);
    std::ifstream sanat(getLibrary());
    std::vector<std::string> words;
    std::string line;
    while (std::getline(sanat, line)) {
        // filter out non-ascii words (first bit 1 in char)
        bool valid = true;
        for (auto c: line) {
            if (c >> 7 != 0)
                valid = false;
        }
        if (valid)
           words.push_back(line);
    }

    // construct solver with words and solve sanajahti
    auto solver = SanajahtiSolver(words, getGrid(), getX(), getY());
    result = solver.solve();
    std::sort(result.begin(),result.end(),longLex);
    fillList();
    list->setEnabled(true);
}

void Window::fillList()
{
    for(auto obj : result)
        list->addItem(QString::fromStdString(std::get<0>(obj)));
}

bool Window::gridFilled()
{
    for(int a=0; a < tiles.length();a++)
        if(tiles.at(a)->text().isEmpty()||tiles.at(a)->text().isNull())
            return false;
    return true;
}

void Window::browse()
{
    library_path->setText(QFileDialog::getOpenFileName(this,tr("Library"), qApp->applicationDirPath(), "All files (*.*)"));
}

void Window::gridChange()
{
    int x=xpanel->text().toInt();
    int y=ypanel->text().toInt();
    if(x<1)
        x=1;
    else if(x>20){
        x=20;
        xpanel->setText(QString::number(x));
    }
    if(y<1)
        y=1;
    else if(y>20){
        y=20;
        ypanel->setText(QString::number(y));
    }
    qDeleteAll(tiles);
    tiles.clear();
    makeGrid(x,y);
}

void Window::drawWord(const QString &word)
{
    std::string std_word=word.toUtf8().constData();
    for(auto obj : result)
        if(obj.first==std_word){
            for(auto obj:tiles){
                obj->setStyleSheet("background-color:grey;"
                                   "color:black");
            }
            for(auto route : obj.second){
                int id=route.first+getX()*route.second;
                tiles.at(id)->setStyleSheet("background-color:green;"
                                            "color:black;");
            }
        }
}

void Window::makeGrid(int x, int y)
{
    for(int a=0;a<x*y;a++){
        tiles.append(addTile(a%x,a/x));
        connect(tiles.last(), SIGNAL(textChanged(const QString &)),mapper, SLOT(map()));
        mapper->setMapping(tiles.last(), a);
        tiles.last()->show();
    }
    if(x<4)
        x=4;
    this->setFixedSize(40*x,115+40*y);
}

std::string Window::getLibrary()
{
    return library_path->text().toUtf8().constData();
}

std::string Window::getGrid()
{
    std::string grid="";
    for(auto obj: tiles)
        grid+=obj->text().toLower().toUtf8().constData();
    return grid;
}

void Window::restart()
{
    restart_button->setHidden(true);
    start_button->setHidden(false);
    xpanel->setDisabled(false);
    ypanel->setDisabled(false);
    list->clear();
    list->setDisabled(true);
    for(auto obj:tiles){
        obj->setStyleSheet("background-color:white");
        obj->setDisabled(false);
    }
}

int Window::getX()
{
    int x=xpanel->text().toInt();
    if(x<0)
        x=1;
    else if(x>20)
        x=20;
    return x;
}

int Window::getY()
{
    int y=ypanel->text().toInt();
    if(y<0)
        y=1;
    else if(y>20)
        y=20;
    return y;
}

QLineEdit* Window::addTile(int x, int y)
{
    QLineEdit* tile = new QLineEdit("",this);
    tile->move(5+40*x,120+40*y);
    tile->setFixedSize(30,30);
    tile->setMaxLength(1);
    QFont font=tile->font();
    font.setPointSize(14);
    tile->setFont(font);
    tile->setAlignment(Qt::AlignCenter);
    return tile;
}

bool longLex(const pair<string, vector<pair<int, int>>>& a,
             const pair<string, vector<pair<int, int>>>& b)
{
    if(a.first.length() == b.first.length())
        for(unsigned j=0;j<a.first.length();j++)
            if(a.first.at(j)!=b.first.at(j))
                return a.first.at(j) < b.first.at(j);
    return a.first.length() > b.first.length();
}
